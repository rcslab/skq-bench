#ifndef EVCOMPAT_EVENT_H
#define EVCOMPAT_EVENT_H

#include <pthread.h>
#include <sys/queue.h>
#include <sys/time.h>

struct event;

LIST_HEAD(evlist, event);

struct event_init_config {
    int eb_flags;
    int data;
};

struct event_base {
    pthread_mutex_t lk;
    pthread_cond_t stop_cv;
    // hashtable of events
    int eb_numev;
    
    // the kqfd for this base
    int eb_kqfd;

    // flags
#define EVB_MULTI 1
    int eb_flags;
};

typedef void (*ev_fn)(int, short, void *);

struct event {
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
    short type;
    ev_fn fn;
    void *arg;

    LIST_ENTRY(event) entry;

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

struct event_base*
event_init_flags(struct event_init_config* confg);

int 
event_del(struct event *ev);

int 
event_add(struct event *ev, const struct timeval *timeout);

void 
event_set(struct event *ev, int fd, short type, ev_fn fn, void *args);

const char*
event_get_version(void);

static inline struct event_base*
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
