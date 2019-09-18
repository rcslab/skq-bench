#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <sys/event.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <machine/atomic.h>

/* these must be included after sys/event.h */
#include "event.h"
#include "internal.h"

static void
ev_wait(struct event *ev)
{
    pthread_cond_wait(&ev->stop_cv, &ev->lk);
}

static void
ev_signal(struct event *ev)
{
    pthread_cond_signal(&ev->stop_cv);
}

static char dump_buf[1024 * 1024 + 1];

void 
event_kq_dump(struct event_base *base)
{
    if (MULT_KQ) {
        if ((base->eb_conf.eb_flags & EVB_MULTI) == EVB_MULTI) {
            int ret;
            uintptr_t args = (uintptr_t)dump_buf;
            //fprintf(stdout, "Userspace buf: %p\n", (void*)args);
            memset(dump_buf, 0, 1024 * 1024 + 1);
            ret = ioctl(base->eb_kqfd, FKQMPRNT, &args);
            if (ret == -1) {
                fprintf(stderr, "print ioctl failed.");
                abort();
            } else {
                fprintf(stdout, "%s\n", dump_buf);
            }
        }
    }
}

static void 
ev_lock(struct event *ev)
{
    pthread_mutex_lock(&ev->lk);
}

static void 
ev_unlock(struct event *ev)
{
    pthread_mutex_unlock(&ev->lk);
}

/* XXX: hack to make unique timer ids */
static int timer_base = -1;
static inline int next_timer_fd()
{
    return atomic_fetchadd_int((volatile uint32_t*)&timer_base, -1);
}

static int 
init_kevent(struct kevent *kev, struct event *ev, int flags, const struct timeval *timeout)
{
    int filter;
    int fflags = 0;
    int64_t data = 0;

    if ((ev->type & EV_READ) == EV_READ) {
        filter = EVFILT_READ;
    } else if ((ev->type & EV_WRITE) == EV_WRITE) {
        filter = EVFILT_WRITE;
    } else if ((ev->type & EV_TIMEOUT) == EV_TIMEOUT) {
        assert(timeout != NULL || ((flags & EV_DELETE) == EV_DELETE));
        filter = EVFILT_TIMER;
        fflags = NOTE_USECONDS;
        if (timeout != NULL) {
            data = timeout->tv_usec + timeout->tv_sec * 1000000;
        }
    } else {
        return EINVAL;
    }

    if ((ev->type & EV_RUNTIME) == EV_RUNTIME) {
        flags |= EV_REALTIME;
    }

    EV_SET(kev, ev->fd, filter, flags, fflags, data, ev);
    return 0;
}

struct event_base *
event_init_flags(struct event_init_config *config)
{
    struct event_base *base = malloc(sizeof(struct event_base));
    assert(base != NULL);

    base->eb_kqfd = kqueue();
    assert(base->eb_kqfd > 0);

    if (config != NULL) {
        memcpy(&base->eb_conf, config, sizeof(struct event_init_config));
        if (MULT_KQ) {
            if ((config->eb_flags & EVB_MULTI) == EVB_MULTI) {
                int ret, kqflag;
                kqflag = config->data;
                ret = ioctl(base->eb_kqfd, FKQMULTI, &kqflag);
                if (ret == -1) {
                    fprintf(stderr, "multikq ioctl failed.");
                    exit(1);
                }

                kqflag = KQTUNE_MAKE(KQTUNE_RTSHARE, config->kq_rshare);
                ret = ioctl (base->eb_kqfd, FKQTUNE, &kqflag);
                if (ret == -1) {
                    fprintf(stderr, "multikq rtshare tune ioctl failed.");
                    exit(1);
                }

                kqflag = KQTUNE_MAKE(KQTUNE_FREQ, config->kq_freq);
                ret = ioctl (base->eb_kqfd, FKQTUNE, &kqflag);
                if (ret == -1) {
                    fprintf(stderr, "multikq freq tune ioctl failed.");
                    exit(1);
                }
            }
        }
    }

    return base;
}

void 
event_config_init(struct event_init_config* conf)
{
    conf->data = 0;
    conf->eb_flags = 0;
    conf->kq_freq = 0;
    conf->kq_rshare = 100;
}

void 
event_base_free(struct event_base *base)
{
    int ret = 0;

    ret = close(base->eb_kqfd);

    free(base);

    if (ret != 0) {
#ifdef DEBUG
    fprintf(stderr, "event_base_free kqfd_free status %d\n", ret);
#endif
    }
}

int 
event_base_set(struct event_base *base, struct event *ev)
{
    if ((ev->state & (EVS_ACTIVE | EVS_PENDING)) != 0) {
        /* the event can't be pending or active */
        return CS_ERR;
    }

    ev->ev_base = base;

    return CS_OK;
}

/* ev lock must be held */
static int
event_del_flags(struct event *ev, int flags)
{
    int ret;
    struct kevent kev;
    struct event_base *base;

    base = ev->ev_base;
    assert(base != NULL);

    init_kevent(&kev, ev, EV_DELETE, NULL);

retry:
    /* ignore non pending events or active requests */
    if ((ev->state & EVS_PENDING) == 0) {
        errno = EINVAL;
        ret = CS_ERR;
        goto end;
    }

    if ((ev->state & EVS_ACTIVE) != 0 && ev->owner != pthread_self()) {
        if (flags == EVENT_DEL_NOBLOCK) {
            errno = EBUSY;
            ret = CS_ERR;
            goto end;
        } else {
            ev_wait(ev);
            goto retry;
        }
    }

    /* remove from kqueue */
    ret = kevent(base->eb_kqfd, &kev, 1, NULL, 0, NULL);

    if (ret != 0) {
        /* skip the removal here */
        errno = ret;
        ret = CS_ERR;
        goto end;
    } else {
        ret = CS_OK;
    }

    /* remove from base */
    ev->state &= ~EVS_PENDING;


    /* XXX: hack to make event inactive after its changed. Memcached requires this
     * IT requires that when an event is active and changed, no more IO is done on the file descriptor
     */
    if ((ev->state & EVS_ACTIVE) != 0) {
        ev->state &= ~EVS_ACTIVE;
        ev->owner = NULL;
        ev->epoch++;
    }

end:
#ifdef DEBUG
    fprintf(stderr, "t %p event_del_flags status %d fd %d\n", (void*)pthread_self(), ret, ev->fd);
#endif
    return ret;
}

int 
event_del(struct event *ev)
{
    int ret;
    ev_lock(ev);
    ret = event_del_flags(ev, EVENT_DEL_BLOCK);
    ev_unlock(ev);
    return ret;
}

int 
event_base_loop(struct event_base *base, int flags)
{
    int ret;
    int fd;
    struct kevent evlist[NEVENT];
    struct event *active[NEVENT];
    struct kevent enlist[NEVENT];
    int epochs[NEVENT];
    int en_cnt;

    int active_cnt;

    struct event *ev;

start:
    fd = 0;
    active_cnt = 0;
    en_cnt = 0;

#ifdef DEBUG
    fprintf(stderr, "event_base_loop t %p kevent base: %p\n", pthread_self(), base);
#endif

    ret = kevent(base->eb_kqfd, NULL, 0, evlist, NEVENT, NULL);

#ifdef DEBUG
    fprintf(stderr, "event_base_loop t %p woke up base: %p #EV: %d\n", pthread_self(), base, ret);
#endif

    /* Don't fail for SIGHUP */
    if (ret < 0 && errno != EINTR) {
#ifdef DEBUG
        fprintf(stderr, "event_base_loop kevent failed for kqfd %d STATUS %d\n", base->eb_kqfd, errno);
#endif
        goto end;
    }

    for (int i = 0; i < ret; i++) {
        fd = evlist[i].ident;
        ev = evlist[i].udata;

        ev_lock(ev);

        /* sanity check */
        assert(ev->fd == fd);
        assert(ev->ev_base == base);

        /* We cannot handle an event that's active, otherwise it implies a bug in our code. */
        assert((ev->state & EVS_ACTIVE) == 0);
        ev->state |= EVS_ACTIVE;

        ev->owner = pthread_self();

        active[active_cnt] = ev;
        epochs[active_cnt] = ev->epoch;
        active_cnt++;

        ev_unlock(ev);
    }

    for (int i = 0; i < active_cnt; i++) {
        ev = active[i];
        ev->fn(ev->fd, 0, ev->arg);
    }

    for (int i = 0; i < active_cnt; i++) {
        ev = active[i];

        ev_lock(ev);
        if (ev->epoch == epochs[i]) {
            ev->state &= ~EVS_ACTIVE;
            ev->owner = NULL;

            if ((ev->type & EV_PERSIST) == 0) {
                /* 
                 * XXX: we could batch all the events and del them in one kevent call 
                 * but requires an event_del function that handles batches 
                 */
                ret = event_del_flags(ev, EVENT_DEL_NOBLOCK);
                assert(ret == CS_OK);
            } else {
                if (!MULT_KQ) {
                    /* Add to enable list to be re-enabled later
                     */
                    memcpy(&enlist[en_cnt], &evlist[i], sizeof(struct kevent));
                    enlist[en_cnt].flags = EV_ENABLE;
                    en_cnt++;     
                }
            }
        } else {
            /* ignore events that have changed */
#ifdef DEBUG
            fprintf(stderr, "t %p event %d changed while active\n", (void*)pthread_self(), ev->fd);
#endif
        }

        /* signal event completion */
        ev_signal(ev);
    }

    if (en_cnt > 0 && !MULT_KQ) {
#ifdef DEBUG
        fprintf(stderr, "t %p event_base_loop re-enabling %d events. First fd: %d\n", (void*)pthread_self(), en_cnt, (int)enlist[0].ident);
#endif
        ret = kevent(base->eb_kqfd, enlist, en_cnt, NULL, 0, NULL);
        /* re-enable events, since events are active, this CANNOT fail */
#ifdef DEBUG
        if (ret != 0) {
            fprintf(stderr, "t %p re-enabling events failed: ret %d, errno %d\n", pthread_self(), ret, errno);
        }
#endif
        assert(ret == 0);
    }
    
    /* unlock all events */
    for(int i = 0; i < active_cnt; i++) {
        ev = active[i];
        ev_unlock(ev);
    }

    if ((flags & EVLOOP_ONCE) == 0)
        goto start;

end:
    return ret;
}

/*
 * Not thread safe w.r.t. each event but thread safe w.r.t. each event base
 * even in a multithreaded setting perhaps no two threads should try to manipulate the same event
 */
int 
event_add(struct event *ev, const struct timeval *timeout)
{
    struct kevent kev;
    struct timespec to;
    struct event_base *base;
    int result;

    base = ev->ev_base;
    assert(base != NULL);

    /*
     * Here we actually allow an active event to be re-added so that apps can manipulate events inside the event handler
     * We just need to guarantee that only one thread handles an event at a time
     * The correctness check is handled by an assert in event_base_loop_ex()
     * 
     * Thus we only check EVS_PENDING but not EVS_ACTIVE
     */
    if ((ev->state & EVS_PENDING) != 0) {
#ifdef DEBUG
        fprintf(stderr, "event_add EVS_PENDING for fd %d\n", ev->fd);
#endif
        errno = EINVAL;
        result = CS_ERR;
        goto end;
    }

    /* patch the timer FD and keeps the timeout info */
    if ((ev->type & EV_TIMEOUT) != 0) {
        ev->fd = next_timer_fd();
    }

    if (timeout != NULL) {
        to.tv_sec = timeout->tv_sec;
        to.tv_nsec = timeout->tv_usec * 1000;
    }

    /* In single KQ mode all events are EV_DISPATCH. Persistent events will be re-enabled 
     * EV_DISPATCH flag is not required for multi kq
     */
    result = init_kevent(&kev, ev, EV_ADD | (MULT_KQ ? 0 : EV_DISPATCH), timeout);

    if (result != 0) {
#ifdef DEBUG
        fprintf(stderr, "event_add init_kevent failed for fd %d\n", ev->fd);
#endif
        errno = result;
        result = CS_ERR;
        goto end;
    }

    ev_lock(ev);

    result = kevent(ev->ev_base->eb_kqfd, &kev, 1, NULL, 0, timeout == NULL ? NULL : &to);

    if (result != 0) {
#ifdef DEBUG
        fprintf(stderr, "event_add kevent failed for fd %d\n", ev->fd);
#endif
        ev_unlock(ev);
        result = CS_ERR;
        goto end;
    }

    ev->state |= EVS_PENDING;

    /* XXX: hack to make event inactive after its changed. Memcached requires that 
     * when an event is active and changed, no more IO is done on the file descriptor
     */
    if ((ev->state & EVS_ACTIVE) != 0) {
        ev->state &= ~EVS_ACTIVE;
        ev->owner = NULL;
        ev->epoch++;
    }

    ev_unlock(ev);
    result = CS_OK;

end:
#ifdef DEBUG
    fprintf(stderr, "t %p event_add status %d for fd %d\n", (void*)pthread_self(), result, ev->fd);
#endif

    return result;
}

void
event_cleanup(struct event* ev)
{
    pthread_mutex_destroy(&ev->lk);
    pthread_cond_destroy(&ev->stop_cv);
}

void 
event_set(struct event *ev, int fd, short type, void (*fn)(int, short, void *), void *arg)
{

    /* 
     * The following is run the 1st time event is set.
     * In case people reuse events inside handler, we want to preserve state, base and owner
     * XXX: 1. libevent API sucks. - No event_destory or event_init, which forces struct event to be stateless
     *                             - Really hard to track down events if the same event is randomly set to have different fds/types.
     *      2. bogus implementation (due to 1). Since stuff are malloced, ev->cv_init can very unlikely be MAGIC.
     */
    if (ev->cv_init != CV_INIT_MAGIC) {
        ev->cv_init = CV_INIT_MAGIC;
        ev->state = 0;
        ev->epoch = 0;
        ev->ev_base = NULL;
        ev->owner = NULL;
        pthread_cond_init(&ev->stop_cv, NULL);
        pthread_mutex_init(&ev->lk, NULL);
    }

    ev_lock(ev);
    /* this is placed after the first "if" to guarantee ev_state is inited for the first time */
    if ((ev->state & EVS_ACTIVE) != 0) {
        if (((ev->state) & EVS_PENDING) != 0) {
            /* cannot change type while the event is queued */
            assert(type == ev->type);
        }
        if ((ev->type & EV_TIMEOUT) == 0) {
            /* Timeout fd doesn't matter */
            assert(fd == ev->fd);
        }
    }

    ev->fd = fd;
    ev->type = type;
    ev->fn = fn;
    ev->arg = arg;
    ev_unlock(ev);
}

const char *
event_get_version(void)
{
    return "2.2.0-compat";
}
