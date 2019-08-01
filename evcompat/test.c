#include <sys/types.h>
#include "event.h"
#include "internal.h"
#include <stdio.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <semaphore.h>
#include <pthread.h>
#include <assert.h>

#define START_TEST(fn) start_test((fn), (#fn))

typedef int (*test_fn)(void);

static int index = 0;

static int within_err(int val, int base, int percent)
{
    assert(base > 0);
    assert(val > 0);
    return (100 * (val - base)) < (percent * base);
}

static void start_test(test_fn fn, const char *name)
{
    index++;
    printf("#%d %s... ",index , name);
    fflush(stdout);
    fn();
    printf("OK!\n");
}

static void err(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    abort();
}

static int g_sockfd[2];

static void init_sockpair()
{
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, &g_sockfd[0]) < 0) {
        err("socket pair");
    }
}

static void free_sockpair()
{
    close(g_sockfd[0]);
    close(g_sockfd[1]);
}

static char
socket_pop(int sockfd)
{
    char buf;

    if (read(sockfd, &buf, 1) < 1) {
        err("socket_pop");
    }

    return buf;
}

static void
socket_push(int sockfd, char ch)
{
    if (write(sockfd, &ch, 1) < 1) {
        err("socket_push");
    }
}

#define BASIC_NUM_PACKETS (1000)

static void test_basic_cb(int fd, short what, void *arg)
{
    UNUSED(what);

    char c;

    c = socket_pop(fd);

    if (c != (char) (*(int *) arg)) {
        err("Wrong char");
    }

#ifdef DEBUG
    printf("Received char code: %d\n", (int) c);
#endif
    (*((int *) arg))++;
}

static int test_basic()
{
    init_sockpair();

    struct event ev;
    struct event_base *eb;
    int num_packets = 0;

    eb = event_init();

    event_set(&ev, g_sockfd[1], EV_READ | EV_PERSIST, test_basic_cb, &num_packets);

    if (event_base_set(eb, &ev) != CS_OK) {
        err("event_base_set");
    }

    if (event_add(&ev, NULL) != CS_OK) {
        err("event_add");
    }

    while (num_packets < BASIC_NUM_PACKETS) {
        int cur = num_packets;
        socket_push(g_sockfd[0], (char) (num_packets));
        event_base_loop(eb, EVLOOP_ONCE);

        while (num_packets != cur + 1) {
        }
    }

    event_del(&ev);
    event_base_free(eb);

    close(g_sockfd[0]);
    close(g_sockfd[1]);

    return 0;
}


#define TIMEOUT_TIME (1)
#define TIMEOUT_LOOP_TIME (5)
#define TIMEOUT_PCTERR (1)

static long get_time()
{
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME_PRECISE, &spec);

    return spec.tv_nsec / 1000 / 1000 + spec.tv_sec * 1000;
}

static void test_timeout_cb(int fd, short what, void *arg)
{
    UNUSED(fd);
    UNUSED(what);

    *(long *) (arg) = get_time();
}

static int test_timeout()
{
    long cur, next;
    struct event ev;
    struct event_base *eb;
    struct timeval timeval;
    timeval.tv_usec = 0;
    timeval.tv_sec = TIMEOUT_TIME;

    eb = event_init();

    for (int i = 0; i < TIMEOUT_LOOP_TIME; i++) {
        
        if (i != 0)
            evtimer_del(&ev);

        evtimer_set(&ev, test_timeout_cb, &next);
        event_base_set(eb, &ev);

        if (evtimer_add(&ev, &timeval) != CS_OK) {
            err("event_add");
        }

        cur = get_time();
        event_base_loop(eb, EVLOOP_ONCE);

        if (!within_err(next - cur, 1000 * TIMEOUT_TIME, TIMEOUT_PCTERR)) {
            err("time mismatch");
        }
    }

    event_base_free(eb);

    return 0;
}

#define TEST_MULT_THR_CNT (16)
#define TEST_MULT_PACKET_MULT (5)
#define TEST_MULT_END_CHAR ('e')

struct thread_info {
    pthread_t thr;
    int tid;
    int ev_cnt;
    int end;
    sem_t *sem;
    struct event_base *eb;
    int *data;
};

static void test_mult_cb(int fd, short what, void *arg)
{
    UNUSED(what);

    char c;
    struct thread_info *info;
    struct thread_info *arr = (struct thread_info *)arg;

    for (int i = 0; i < TEST_MULT_THR_CNT; i++) {
        if (arr[i].thr == pthread_self()) {
            info  = &arr[i];
        }
    }
    c = socket_pop(fd);

    if (c == TEST_MULT_END_CHAR) {
        info->end = 1;
    } else {
        info->ev_cnt++;
        sem_post(info->sem);
    }
#ifdef DEBUG
    printf("Thread %d received char %c from fd %d\n", info->tid, c, fd);
#endif
}

static void *test_mult_thread(void *arg)
{
    struct thread_info *info = (struct thread_info *) arg;
    while (!info->end) {
        event_base_loop(info->eb, EVLOOP_ONCE);
    }

    return NULL;
}

static int test_mult()
{
    sem_t sem;
    struct event ev;
    struct event_base *eb;
    struct event_init_config cfg;
    struct thread_info tinfo[TEST_MULT_THR_CNT];
    int num_packets = 0;
    int sum = 0;

    init_sockpair();
    sem_init(&sem, 0, 0);
    cfg.data = 0;
    cfg.eb_flags = EVB_MULTI;


    eb = event_init_flags(&cfg);
    event_set(&ev, g_sockfd[1], EV_READ | EV_PERSIST, test_mult_cb, tinfo);

    if (event_base_set(eb, &ev) != CS_OK) {
        err("event_base_set");
    }

    if (evtimer_add(&ev, NULL) != CS_OK) {
        err("event_add");
    }

    for (int i = 0; i < TEST_MULT_THR_CNT; i++) {
        tinfo[i].sem = &sem;
        tinfo[i].end = 0;
        tinfo[i].ev_cnt = 0;
        tinfo[i].eb = eb;
        tinfo[i].tid = i;
        tinfo[i].data = (void*)tinfo;
        pthread_create(&tinfo[i].thr, NULL, test_mult_thread, &tinfo[i]);
    }

    while (num_packets < TEST_MULT_THR_CNT * TEST_MULT_PACKET_MULT) {
#ifdef DEBUG
        printf("Writing to socket %d\n", num_packets);
#endif 
        socket_push(g_sockfd[0], 'c');
        sem_wait(&sem);
        num_packets++;
    }

    for (int i = 0; i < TEST_MULT_THR_CNT; i++) {
#ifdef KQ_SCHED_SUPPORT
        if (tinfo[i].ev_cnt != TEST_MULT_PACKET_MULT) {
            perror("evcnt doesn't match");
        }
#endif
        sum += tinfo[i].ev_cnt;
    }

    if (sum != TEST_MULT_THR_CNT * TEST_MULT_PACKET_MULT) {
        err("total evcnt doesn't match");
    }

    for (int i = 0; i < TEST_MULT_THR_CNT; i++) {
        socket_push(g_sockfd[0], TEST_MULT_END_CHAR);
    }

    for (int i = 0; i < TEST_MULT_THR_CNT; i++) {
        pthread_join(tinfo[i].thr, NULL);
    }

    event_del(&ev);
    event_base_free(eb);

    free_sockpair();

    return 0;
}

/* EV_DELETE */
static void test_delete_cb(int fd, short what, void *arg)
{
    UNUSED(fd);
    UNUSED(what);

    (*(int *) (arg))++;
#ifdef DEBUG
    printf("test_delete_cb called.\n");
#endif
}

static void *test_delete_thread(void *arg)
{
    struct thread_info *tinfo = (struct thread_info *) arg;
    while (*tinfo->data < 6) {
        event_base_loop(tinfo->eb, EVLOOP_ONCE);
    }

    return NULL;
}

static int test_delete()
{
    /* This is changed from another thread. */
    volatile int num = 0;
    int prev;
    int ret;
    struct event ev;
    struct event_base *eb;
    struct timeval timeval;
    struct thread_info tinfo;
    timeval.tv_usec = 0;
    timeval.tv_sec = 1;

    eb = event_init();
    event_set(&ev, 0, EV_PERSIST | EV_TIMEOUT, test_delete_cb, (int*)&num);

    if (event_base_set(eb, &ev) != CS_OK) {
        err("event_base_set");
    }

    if (event_add(&ev, &timeval) != CS_OK) {
        err("event_add");
    }

    // create thread
    tinfo.eb = eb;
    tinfo.data = (int*)&num;
    tinfo.end = 0;
    pthread_create(&tinfo.thr, NULL, test_delete_thread, &tinfo);

    // make sure the event works before deletion
    while (num < 3) {}

#ifdef DEBUG
    printf("Deleting event...\n");
#endif

    ret = evtimer_del(&ev);
    if (ret != CS_OK) {
        err("ev_delete");
    }
    prev = num;

    sleep(3);

    // make sure the event doesn't work after deletion
    if (num != prev) {
        err("ev triggered after deletion");
    }

#ifdef DEBUG
    printf("Re-adding event...\n");
#endif
    // make sure the event works after re-addition
    if (event_add(&ev, &timeval) != CS_OK) {
        err("event_add");
    }

    while(num < 6) {}
    pthread_join(tinfo.thr, NULL);

    evtimer_del(&ev);
    event_base_free(eb);

    return 0;
}


/* EV_DELETE + BLOCKING */
static void test_deleteb_cb(int fd, short what, void *arg)
{
    UNUSED(fd);
    UNUSED(what);

    sleep(3);
    (*(int*)arg)++;
}

static void *test_deleteb_thread(void *arg)
{
    struct thread_info *tinfo = (struct thread_info *) arg;
    event_base_loop(tinfo->eb, EVLOOP_ONCE);
    return NULL;
}

static int test_deleteb()
{
    int num = 0;
    int ret;
    struct event ev;
    struct event_base *eb;
    struct thread_info tinfo;
    init_sockpair();

    eb = event_init();
    event_set(&ev, g_sockfd[1], EV_PERSIST | EV_READ, test_deleteb_cb, &num);

    if (event_base_set(eb, &ev) != CS_OK) {
        err("event_base_set");
    }

    if (event_add(&ev, NULL) != CS_OK) {
        err("event_add");
    }

    // create thread
    tinfo.eb = eb;
    tinfo.end = 0;
    pthread_create(&tinfo.thr, NULL, test_deleteb_thread, &tinfo);

    socket_push(g_sockfd[0], '0');

    // wait 1 second for information to propogate
    sleep(1);
    if (num != 0) {
        err("num != 0");
    }
#ifdef DEBUG
        printf("deleting event blocking");
#endif
    ret = event_del(&ev);
#ifdef DEBUG
        printf("deleted event blocking");
#endif
    if ((ret != CS_OK) || (num != 1)) {
        err("ret || num != 1");
    }

    pthread_join(tinfo.thr, NULL);

    event_base_free(eb);
    free_sockpair();
    return 0;
}


/* test owner */
#define TEST_OWNER_READD (4)
static void test_owner_cb(int fd, short what, void *arg)
{
    UNUSED(fd);
    UNUSED(what);

    struct thread_info * tinfo = (struct thread_info *)arg;
    tinfo->ev_cnt++;
    socket_pop(g_sockfd[1]);
    if(event_del((struct event *)tinfo->data) != CS_OK) {
        err("event_del inside handler");
    }
    if (tinfo->ev_cnt < TEST_OWNER_READD) {
        if(event_add((struct event *)tinfo->data, NULL) != CS_OK) {
            err("event_add inside handler");
        }
    }
}

static void test_owner_tcb(int fd, short what, void *arg)
{
    UNUSED(fd);
    UNUSED(what);
    
    struct thread_info * tinfo = (struct thread_info *)arg;
    tinfo->ev_cnt++;
    tinfo->end = 1;
}

static void *test_owner_thread(void *arg)
{
    struct thread_info *tinfo = (struct thread_info *) arg;
    while(!tinfo->end) {
        event_base_loop(tinfo->eb, EVLOOP_ONCE);
    }
    return NULL;
}

static int test_owner()
{
    struct event ev, evt;
    struct event_base *eb;
    struct thread_info tinfo;
    int t_sockfd[2];
    
    init_sockpair();

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, &t_sockfd[0]) < 0) {
        err("socket pair");
    }

    eb = event_init();
    
    /* Not persist to check EV_CHANGED works properly */
    event_set(&ev, g_sockfd[1], EV_READ, test_owner_cb, &tinfo);
    event_set(&evt, t_sockfd[1], EV_PERSIST | EV_READ, test_owner_tcb, &tinfo);

    if (event_base_set(eb, &ev) != CS_OK || event_base_set(eb, &evt) != CS_OK) {
        err("event_base_set");
    }

    if (event_add(&ev, NULL) != CS_OK || event_add(&evt, NULL) != CS_OK) {
        err("event_add");
    }

    // create thread
    tinfo.eb = eb;
    tinfo.data = (int*)&ev;
    tinfo.ev_cnt = 0;
    tinfo.end = 0;
    pthread_create(&tinfo.thr, NULL, test_owner_thread, &tinfo);

    for (int i = 0; i < TEST_OWNER_READD; i++) {
        socket_push(g_sockfd[0], '0');
        /* wait for the handler to run */
        sleep(1);
        if (tinfo.ev_cnt != i + 1) {
            err("tinfo.ev_cnt");
        }
    }

    socket_push(g_sockfd[0], '0');
    /* wait for the handler to run */
    sleep(1);
    
    /* Make sure the socket push doesn't trigger an event */
    if (tinfo.ev_cnt != TEST_OWNER_READD) {
        err("tinfo.ev_cnt increased");
    }

    socket_push(t_sockfd[0], '0');
    pthread_join(tinfo.thr, NULL);

    /* Make sure the tmp socket push triggers an event */
    if (tinfo.ev_cnt != TEST_OWNER_READD + 1) {
        err("tinfo.ev_cnt didn't increased");
    }

    if (event_del(&evt) != CS_OK) {
        err("event_del");
    }
    
    event_base_free(eb);
    free_sockpair();
    return 0;
}

/* Test owner timeout */

#define TOT_CNT (6)
static struct timeval tot_val;

static void* test_owner_timeout_thread(void *arg)
{
    int cnt = 0;
    long last, cur;
    struct thread_info *tinfo = (struct thread_info *) arg;
    struct event *ev = (struct event *)tinfo->data;

    if (evtimer_add(ev, &tot_val) != CS_OK) {
        err("evtimer_add");
    }

    while(cnt < TOT_CNT) {
        last = get_time();
        event_base_loop(tinfo->eb, EVLOOP_ONCE);
        cur = get_time();
        if (!within_err(cur - last, TIMEOUT_TIME * 1000, TIMEOUT_PCTERR)) {
            err("timer err too large : %d, %d", cur - last, TIMEOUT_TIME);
        }
        cnt++;
    }

    return NULL;
}

static void test_owner_timeout_cb(int fd, short what, void *arg)
{
    UNUSED(fd);
    UNUSED(what);
    struct thread_info *tinfo = (struct thread_info *)arg;
    struct event *ev = (struct event *)tinfo->data;
    if (evtimer_del(ev) != CS_OK) {
        err("evtimer_del");
    }
    
    evtimer_set(ev, test_owner_timeout_cb, tinfo);
    event_base_set(tinfo->eb, ev);

    if (evtimer_add(ev, &tot_val) != CS_OK) {
        err("evtimer_add");
    }
}

static int test_owner_timeout()
{
    struct event ev;
    struct event_base *eb;
    tot_val.tv_usec = 0;
    tot_val.tv_sec = TIMEOUT_TIME;
    struct thread_info tinfo;

    eb = event_init();
    if (eb == NULL) {
        err("event_init is null");
    }

    evtimer_set(&ev, test_owner_timeout_cb, &tinfo);
    event_base_set(eb, &ev);

    tinfo.data = (void*)&ev;
    tinfo.eb = eb;
    pthread_create(&tinfo.thr, NULL, test_owner_timeout_thread, &tinfo);

    pthread_join(tinfo.thr, NULL);

    event_base_free(eb);

    return 0;
}

int main()
{
    START_TEST(test_basic);
    START_TEST(test_delete);
    START_TEST(test_deleteb);
    START_TEST(test_owner);
    START_TEST(test_timeout);
    START_TEST(test_owner_timeout);
    START_TEST(test_mult);

    printf("All tests finished!\n");
    return 0;
}