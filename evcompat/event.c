#include "event.h"
#include "internal.h"
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
#include <sys/types.h>
#include <machine/atomic.h>
#include "ck_ht_hash.h"

static void
ev_wait(struct event_base *eb)
{
    pthread_cond_wait(&eb->stop_cv, &eb->lk);
}

static void
ev_signal(struct event_base *eb)
{
    pthread_cond_signal(&eb->stop_cv);
}

static void 
evb_lock(struct event_base *base)
{
    pthread_mutex_lock(&base->lk);
}

static void 
evb_unlock(struct event_base *base)
{
    pthread_mutex_unlock(&base->lk);
}

/* XXX: hack to make unique timer ids */
static int timer_base = -1;
static inline int next_timer_fd()
{
    return atomic_fetchadd_int((volatile uint32_t*)&timer_base, -1);
}

static uint32_t 
fdhash(int fd)
{
    uint32_t out;

    MurmurHash3_x86_32((void*) &fd, sizeof(int), HASHTBL_SEED, &out);

    return out % HASHTBL_SZ;
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

    EV_SET(kev, ev->fd, filter, flags, fflags, data, NULL);
    return 0;
}


static struct event *
find_event(struct evlist *head, int fd)
{
    struct event *ret = NULL, *temp;
    LIST_FOREACH(temp, head, entry) {
        if (temp->fd == fd) {
            ret = temp;
            break;
        }
    }
    return ret;
}

struct event_base *
event_init_flags(struct event_init_config *config)
{
    struct event_base *base = malloc(sizeof(struct event_base));
    assert(base != NULL);
    base->eb_evtbl = malloc(sizeof(struct evlist) * HASHTBL_SZ);
    assert(base->eb_evtbl != NULL);
    pthread_mutex_init(&base->lk, NULL);
    for (int i = 0; i < HASHTBL_SZ; i++) {
        LIST_INIT(&base->eb_evtbl[i]);
    }

    base->eb_kqfd = kqueue();
    assert(base->eb_kqfd > 0);

    base->eb_numev = 0;

    if (config != NULL) {
        base->eb_flags = config->eb_flags;
        if ((config->eb_flags & EVB_MULTI) == EVB_MULTI) {
#ifdef KQ_SCHED_SUPPORT
            int ret;
            kqflag = config->data;
            ret = ioctl(base->eb_kqfd, FKQMULTI, &kqflag);
            assert(ret != -1);
#endif
        }
    }

    return base;
}

void 
event_base_free(struct event_base *base)
{
    int ret = 0;
    free(base->eb_evtbl);
    pthread_mutex_destroy(&base->lk);

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

/* ev base lock must be held */
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
            ev_wait(base);
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

    /* remove from base queues */
    ev->state &= ~EVS_PENDING;
    LIST_REMOVE(ev, entry);
    base->eb_numev--;
    
    /* set changed flag */
    if ((ev->state & EVS_ACTIVE) != 0) {
        ev->state |= EVS_CHANGED;
#ifdef DEBUG
        fprintf(stderr, "event_del_flags EVS_CHANGED for fd %d\n", ev->fd);
#endif
    }
end:
#ifdef DEBUG
    fprintf(stderr, "event_del_flags status %d fd %d\n", ret, ev->fd);
#endif
    return ret;
}

int 
event_del(struct event *ev)
{
    int ret;
    assert(ev->ev_base != NULL);
    evb_lock(ev->ev_base);
    ret = event_del_flags(ev, EVENT_DEL_BLOCK);
    evb_unlock(ev->ev_base);
    return ret;
}

int 
event_base_loop_ex(struct event_base *base, void *args, int flags)
{
    void *real_arg;
    int ret;
    int fd;
    int hash;
    struct kevent evlist[NEVENT];
    struct event *active[NEVENT];
    struct kevent enlist[NEVENT];
    int en_cnt;

    int active_cnt;

    struct event *ev;

start:
    fd = 0;
    active_cnt = 0;
    en_cnt = 0;
    evb_lock(base);
    if (base->eb_numev == 0) {
        errno = EINVAL;
        ret = CS_WARN;
        evb_unlock(base);
        goto end;
    }
    evb_unlock(base);

#ifdef DEBUG
    fprintf(stderr, "event_base_loop_ex thread %p kevent base: %p\n", pthread_self(), base);
#endif

    ret = kevent(base->eb_kqfd, NULL, 0, evlist, NEVENT, NULL);

#ifdef DEBUG
    fprintf(stderr, "event_base_loop_ex thread %p woke up base: %p\n", pthread_self(), base);
#endif

    if (ret < 0) {
#ifdef DEBUG
        fprintf(stderr, "event_base_loop_ex kevent failed for kqfd %d STATUS %d\n", base->eb_kqfd, ret);
#endif
        errno = ret;
        ret = CS_ERR;
        goto end;
    }

    /* traverse the list of events for that event */
    evb_lock(base);
    for (int i = 0; i < ret; i++) {
        fd = evlist[i].ident;
        hash = fdhash(fd);

        ev = find_event(&base->eb_evtbl[hash], fd);
        if (ev != NULL) {
            
            /* We cannot handle an event that's active, otherwise it implies a bug in our code. */
            assert((ev->state & EVS_ACTIVE) == 0);

            ev->state |= EVS_ACTIVE;
            ev->owner = pthread_self();
            active[active_cnt] = ev;
            active_cnt++;
        } else {
#ifdef DEBUG
            fprintf(stderr, "event_base_loop_ex no matching struct event for fd %d\n", fd);
#endif
        }
    }
    evb_unlock(base);

    for (int i = 0; i < active_cnt; i++) {
        ev = active[i];
        real_arg = ((base->eb_flags & EVB_MULTI) == 0) ? ev->arg : args;
        ev->fn(fd, 0, real_arg);
    }

    evb_lock(base);
    for (int i = 0; i < active_cnt; i++) {
        ev = active[i];
        ev->owner = NULL;
        ev->state &= ~EVS_ACTIVE;

        if ((ev->state & EVS_CHANGED) == 0) {
            /* Delete non-persistent events */
            if ((ev->type & EV_PERSIST) == 0) {
                /* 
                 * XXX: we could batch all the events and del them in one kevent call 
                 * but requires an event_del function that handles batches 
                 */
                 ret = event_del_flags(ev, EVENT_DEL_NOBLOCK);
                assert(ret == CS_OK);
            } else {
                /* Add to enable list to be re-enabled later */
                memcpy(&enlist[en_cnt], &evlist[i], sizeof(struct kevent));
                enlist[en_cnt].flags = EV_ENABLE;
                en_cnt++;            
            }
        } else {
            /* Ignore events which are changed inside the handler function */
            ev->state &= ~EVS_CHANGED;
        }

        /* wakeup threads waiting on the event */
        ev_signal(base);
    }

    if (en_cnt > 0) {
#ifdef DEBUG
        fprintf(stderr, "event_base_loop_ex re-enabling %d events\n", en_cnt);
#endif
        ret = kevent(base->eb_kqfd, enlist, en_cnt, NULL, 0, NULL);
        /* re-enable events, since events are active, this CANNOT fail */
        assert(ret == 0);
    }

    evb_unlock(base);

    if ((flags & EVLOOP_ONCE) == 0)
        goto start;

end:
    return ret;
}

int 
event_base_loop(struct event_base *base, int flags)
{
    return event_base_loop_ex(base, NULL, flags);
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

    uint32_t hash = fdhash(ev->fd);
    assert(hash < HASHTBL_SZ);

    if (timeout != NULL) {
        to.tv_sec = timeout->tv_sec;
        to.tv_nsec = timeout->tv_usec * 1000;
    }

    /* All events are EV_DISPATCH. Persistent events will be re-enabled */
    result = init_kevent(&kev, ev, EV_ADD | EV_DISPATCH, timeout);

    if (result != 0) {
#ifdef DEBUG
        fprintf(stderr, "event_add init_kevent failed for fd %d\n", ev->fd);
#endif
        errno = result;
        result = CS_ERR;
        goto end;
    }

    evb_lock(base);

    result = kevent(ev->ev_base->eb_kqfd, &kev, 1, NULL, 0, timeout == NULL ? NULL : &to);

    if (result != 0) {
#ifdef DEBUG
        fprintf(stderr, "event_add kevent failed for fd %d\n", ev->fd);
#endif
        evb_unlock(base);
        result = CS_ERR;
        goto end;
    }

    base->eb_numev++;
    LIST_INSERT_HEAD(&base->eb_evtbl[hash], ev, entry);
    ev->state |= EVS_PENDING;
    
    /* set changed flag */
    if ((ev->state & EVS_ACTIVE) != 0) {
        ev->state |= EVS_CHANGED;
#ifdef DEBUG
        fprintf(stderr, "event_add EVS_CHANGED for fd %d\n", ev->fd);
#endif
    }
    evb_unlock(base);
    result = CS_OK;

    end:
#ifdef DEBUG
    fprintf(stderr, "event_add status %d for fd %d\n", result, ev->fd);
#endif

    return result;
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
        ev->ev_base = NULL;
        ev->owner = NULL;
    }

    /* after the first "if" to guarantee ev_state is inited for the first time */
    if ((ev->state & EVS_ACTIVE) != 0) {
        /* Can't change these while active or undefined behavior */
        assert(fd == ev->fd);
        assert(type == ev->type);
    }

    ev->fd = fd;
    ev->type = type;
    ev->fn = fn;
    ev->arg = arg;
}

const char *
event_get_version(void)
{
    return "2.2.0-compat";
}
