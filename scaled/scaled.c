#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/event.h>
#include <sys/sysctl.h>
#include <sys/param.h>
#include <sys/cpuset.h>

#include <pthread.h>
#include <pthread_np.h>
#include <unistd.h>
#include <signal.h>
#include <err.h>
#include <netdb.h>

// #include <iostream>
// #include <vector>
// #include <thread>
// #include <set>
// #include <unordered_map>

struct options {
	int conn_count;
	int thread_cnt;
	int single_kqueue;
	int nevent;
	int verbose;
	int load;
	int time;
};

struct thread_counter {
	char pad1[64];
	int cnt;
	char pad2[64];
};

struct thread_info {
	int tid;
	pthread_t tfd;
	int kq;
	struct kevent * kev;
	struct thread_counter stat;
};

static struct options options = {.conn_count = 1,
							     .thread_cnt = 1,
								 .single_kqueue = 0,
								 .nevent = 1,
								 .verbose = 0,
								 .load = 0,
								 .time = 5};
static int exit_pipes[2];

#define W(fmt, ...) do { \
	fprintf(stderr, "[WARN] " fmt, ##__VA_ARGS__); \
} while(0)

#define E(fmt, ...) do { \
	fprintf(stderr, "[ERROR] " fmt, ##__VA_ARGS__); \
	exit(1); \
} while(0)

#define V(fmt, ...) do { \
	if (options.verbose) { \
	fprintf(stdout, "[INFO] " fmt, ##__VA_ARGS__); \
	} \
} while(0)

static uint64_t
get_time_us() 
{
  	struct timespec ts;
  	clock_gettime(CLOCK_REALTIME, &ts);
  	//  clock_gettime(CLOCK_REALTIME, &ts);
  	return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static int
get_ncpu()
{
    int mib[4];
    int numcpu;
    size_t len = sizeof(numcpu);

    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;

    sysctl(mib, 2, &numcpu, &len, NULL, 0);

    if (numcpu < 1)
    {
        E("< 1 cpu detected\n");
        exit(1);
    }

    return numcpu;
}

static int ncpu = 0;
static int curcpu;
/* pin to non-hyperthreads then to hyperthreads*/
static int 
get_next_cpu()
{
    int ret;

    if (ncpu == 0) {
        ncpu = get_ncpu();
    }

    if (ncpu == 1) {
        return 0;
    }
    
    if(curcpu >= ncpu) {
        curcpu = curcpu % ncpu;
    }
    
    ret = curcpu;
    curcpu = curcpu + 2;

    return ret;
}

static void *
worker_thread(void * _info)
{
	struct thread_info * info = _info;
	int ret;
	int nchanges = 0;
	V("Thread %d created...\n", info->tid);
	int stop = 0;

	while(!stop) {
		ret = kevent(info->kq, info->kev, nchanges, info->kev, options.nevent, NULL);
		if (ret == -1) {
			E("kevent returned error %d", errno);
		}

		V("Thread %d got %d events.\n", info->tid, ret);

		for(int i = 0; i < ret; i++) {
			struct kevent* ev = &info->kev[i];
			V("Thread %d processing fd %d.\n", info->tid, (int)ev->ident);
			
			if (ev->ident != exit_pipes[0]) {
				uint64_t cur = get_time_us();
					while(get_time_us() < cur + options.load) {
				}

				info->stat.cnt++;

#ifndef FKQMULTI
				if (options.single_kqueue) {
					ev->flags = EV_ENABLE;
				}
#endif
			} else {
				stop = 1;
			}
		}

#ifndef FKQMULTI
		if (options.single_kqueue) {
			nchanges = ret;
		}
#endif
	}

	V("Thread %d exiting...\n", info->tid);
	return NULL;
}

static void usage() 
{
	fprintf(stdout, "Usage:\n"
					"    -c: pipe count.\n"
					"    -w: total time.\n"
					"    -s: single kqueue/SKQ.\n"
					"    -n: nevent.\n"
					"    -v: verbose.\n"
					"    -l: request load in us.\n"
					"    -t: total threads.\n"
					"    -h: show help.\n\n"
					);
}

static void dump_options()
{
	V("Config:\n"
					"    Pipe count: %d\n"
					"    Thread count: %d\n"
					"    Total time: %d\n"
					"    Request load: %d\n"
					"    nevent: %d\n"
					"    single kqueue/SKQ: %d\n",
					options.conn_count,
					options.thread_cnt,
					options.time,
					options.load,
					options.nevent,
					options.single_kqueue
					);
}

int
main(int argc, char* argv[]) 
{
	int ch;

	while ((ch = getopt(argc, argv, "c:w:sn:vl:t:h")) != -1) {
		switch (ch) {
			case 'c':
				options.conn_count = atoi(optarg);
				break;
			case 'w':
				options.time = atoi(optarg);
				break;
			case 's':
				options.single_kqueue = 1;
				break;
			case 'n':
				options.nevent = atoi(optarg);
				break;
			case 'v':
				options.verbose = 1;
				W("Verbose mode can cause SUBSTANTIAL latency fluctuations in some(XFCE) terminals!\n");
				break;
			case 'l':
				options.load = atoi(optarg);
				break;
			case 't':
				options.thread_cnt = atoi(optarg);
				break;
			case 'h':
				usage();
				exit(0);
			default:
				E("Unrecognized option - %c\n\n", ch);
		}
	}

	signal(SIGPIPE, SIG_IGN);

	dump_options();

	struct thread_info * tinfo = malloc(sizeof(struct thread_info) * options.thread_cnt);

	// create all kqueues
	V("Creating Kqueues/SKQs...\n");
	int kq;
	if (options.single_kqueue) {
		kq = kqueue();
		if (kq == -1) {
			E("kqueue() failed: %d\n", errno);
		}
#ifdef FKQMULTI
		int sched = 2;
		V("Setting SKQ sched = CPU0\n");
		if(ioctl(kq, FKQMULTI, &sched) == -1) {
			E("ioctl() failed: %d\n", errno);
		}
#endif
	}

	for (int i = 0; i < options.thread_cnt; i++) {
		if (options.single_kqueue) {
			tinfo[i].kq = kq;
		} else {
			tinfo[i].kq = kqueue();
			if (tinfo[i].kq == -1) {
				E("kqueue() failed: %d\n", errno);
			}
		}
	}

	// create all pipe pairs
	V("Creating pipes...\n");
	struct kevent kev;

	if (pipe(&exit_pipes[0]) == -1) {
		E("pipe() failed: %d\n", errno);
	}
	kev.filter = EVFILT_READ;
	kev.ident = exit_pipes[0];
	kev.flags = EV_ADD;
	if (options.single_kqueue) {
		if (kevent(kq, &kev, 1, NULL, 0, NULL) == -1) {
			E("kevent() failed: %d\n", errno);
		}
	} else {
		for(int i = 0; i < options.thread_cnt; i++) {
			if (kevent(tinfo[i].kq, &kev, 1, NULL, 0, NULL) == -1) {
				E("kevent() failed: %d\n", errno);
			}
		}
	}

	int * pipes = malloc(sizeof(struct thread_info) * options.conn_count * 2);
	char c = 'a';
	for(int i = 0; i < options.conn_count; i++) {
		if (pipe(&pipes[i * 2]) == -1) {
			E("pipe() failed: %d\n", errno);
		}
		
		if (write(pipes[i * 2 + 1],&c, 1) != 1) {
			E("write() failed: %d\n", errno);
		}

		kev.ident = pipes[i * 2];
		kev.filter = EVFILT_READ;
		kev.flags = EV_ADD;

#ifndef FKQMULTI
		if (options.single_kqueue) {
			kev.flags |= EV_DISPATCH;
		}
#endif

		if (kevent(tinfo[i % options.thread_cnt].kq, &kev, 1, NULL, 0, NULL) == -1) {
			E("kevent() failed: %d\n", errno);
		}
	}

	// create all threads
	V("Creating all threads...\n");
	for (int i = 0; i < options.thread_cnt; i++) {
		pthread_attr_t  attr;
		pthread_attr_init(&attr);


		int tgt;
		cpuset_t cpuset;

		tgt = get_next_cpu();
	
		V("Setting thread %d affinity to CPU %d %d\n", i, tgt, tgt + 1);

		CPU_ZERO(&cpuset);
		CPU_SET(tgt, &cpuset);
		CPU_SET(tgt + 1, &cpuset);

		if (pthread_attr_setaffinity_np(&attr, sizeof(cpuset), &cpuset) != 0) {
				E("Can't set affinity: %d\n", errno);
		}

		tinfo[i].kev = malloc(options.nevent * sizeof(struct kevent));
		tinfo[i].stat.cnt = 0;
		tinfo[i].tid = i;
		pthread_create(&tinfo[i].tfd, &attr, worker_thread, &tinfo[i]);
		V("Thread %d created...\n", tinfo[i].tid);
	}

	sleep(options.time);

	int qps = 0;
	for (int i = 0; i < options.thread_cnt; i++) {
		qps += tinfo[i].stat.cnt;
	}

	write(exit_pipes[1], &ch, 1);

	for (int i = 0; i < options.thread_cnt; i++) {
		pthread_join(tinfo[i].tfd, NULL);
	}

	printf("%.1f\n", (float)qps / (float)options.time);
	
	return 0;
}
