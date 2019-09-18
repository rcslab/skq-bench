#ifndef EVCOMPAT_EVENT_H
#define EVCOMPAT_EVENT_H

#include <pthread.h>
#include <sys/time.h>

struct event;

struct event_init_config {
#define EVB_MULTI 1
    int eb_flags;
    int kq_rshare;
    int kq_freq;
    int data;
};

struct event_base {
    // the kqfd for this base
    int eb_kqfd;

    // flags
    struct event_init_config eb_conf;
};

typedef void (*ev_fn)(int, short, void *);

struct event {
    pthread_cond_t stop_cv;
    pthread_mutex_t lk;
    struct event_base *ev_base;
    
    /* XXX: change the name of this var */
    unsigned int cv_init;
    /* event's owner */
    pthread_t owner;

    int fd;
#define EV_READ (0x1)
#define EV_WRITE (0x2)
#define EV_TIMEOUT (0x4)
#define EV_PERSIST (0x8)
#define EV_RUNTIME (0x10)
    short type;
    ev_fn fn;
    void *arg;

    int state;
    int epoch; /* incremeneted whenever an active event is added/deleted  */
};

int 
event_base_set(struct event_base *base, struct event *ev);

void 
event_base_free(struct event_base *base);

#define EVLOOP_ONCE 0x1

int 
event_base_loop(struct event_base *base, int flags);

struct event_base *
event_init_flags(struct event_init_config* confg);

void
event_kq_dump(struct event_base *base);

int 
event_del(struct event *ev);

int 
event_add(struct event *ev, const struct timeval *timeout);

void 
event_set(struct event *ev, int fd, short type, ev_fn fn, void *args);

void
event_cleanup(struct event* ev);

const char*
event_get_version(void);

void 
event_config_init(struct event_init_config* conf);

static inline struct event_base *
event_init()
{
    return event_init_flags(NULL);
}

static inline int 
evtimer_add(struct event* ev, const struct timeval* tv)
{
    return event_add(ev, tv);
}

static inline void 
evtimer_set(struct event* ev, ev_fn cb, void* arg)
{
    event_set(ev, -1, EV_TIMEOUT, cb, arg);
}

static inline int 
evtimer_del(struct event* ev)
{
    return event_del(ev);
}


#endif
