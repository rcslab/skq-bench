#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <err.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/event.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread_np.h>
#include <random>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <vector>

#include "../common.h"

using namespace std;

#define NEVENT (256)
#define SOCK_BACKLOG (10000)

struct server_option {
	int threads;
	int port;
	int skq;
	int skq_flag;
	int cpu_affinity;
	int skq_dump;
	int verbose;
	int delay;
	int conn_delay;
};

struct conn_hint {
	int delay;
};

struct worker_thread {
	pthread_t thrd;
	int kqfd;
	long evcnt;
	int id;
	char _pad[64];
};

struct server_option options = {.threads = 1, 
								.port = DEFAULT_SERVER_CLIENT_CONN_PORT, 
								.skq = 0, 
								.skq_flag = 0, 
								.cpu_affinity = 0,
								.skq_dump = 0,
								.verbose = 0,
								.delay = 0,
								.conn_delay = 0};

static long perf_sample[1000] = {0};
static volatile long perf_avg;

/* XXX: legacy BS */
static const int enable = 1;

/* cpu assignment */
static int ncpu = 0;
static int cur = 0;

static void 
server_socket_prepare(vector<int> *socks) 
{
	struct sockaddr_in server_addr;
	int status;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(options.port);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	for (int i = 0; i < 1; i++) {
		int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (fd < 0) {
			perror("server listen socket");
			exit(1);
		}

		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
			E("server listen setsockopt reuseaddr");
		}

		if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) < 0) {
			E("server listen setsockopt reuseport");
		}

		if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable)) < 0) {
			E("server listen setsockopt NODELAY");
		}

		status = ::bind(fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
		if (status < 0) {
			perror("server listen bind");
			exit(1);
		}

        status = listen(fd, SOCK_BACKLOG);
		if (status < 0) {
			perror("listen");
			exit(1);
		}

		socks->push_back(fd);
	}
}

static void
drop_conn(struct worker_thread *tinfo, struct kevent *kev)
{
	int status;
	int conn_fd = kev->ident;
	struct kevent ev;
	EV_SET(&ev, conn_fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
	
	status = kevent(tinfo->kqfd, &ev, 1, 0, 0, NULL);

	if (status < 0) {
		E("Failed to delete connection %d from kqueue\n", conn_fd);
	}

	close(conn_fd);

	free(kev->udata);
}

static void
build_delay_table(vector<int> *tbl)
{
	/* 95 + 4 + 1 = 100 */

	tbl->push_back(200);

	for(int i = 0; i < 4; i++) {
		tbl->push_back(50);
	}

	for(int i = 0; i < 95; i++) {
		tbl->push_back(20);
	}
}

#define RM (1000)
static int 
get_next_delay()
{
	int r = rand() % RM;
	if (r <= RM / 100 * 95) {
		/* 95% low */
		return 20;
	} else if (r <= RM / 100 * 99) {
		/* 4% mid */
		return 50;
	} else {
		/* 1% high */
		return 200;
	}
}

static void
handle_event(struct worker_thread *tinfo, struct kevent* kev)
{
	int conn_fd;
	int status;
	char data[MESSAGE_LENGTH + 1] = {0};

	if (kev->filter != EVFILT_READ) {
		E("Unknown event filter %d\n", kev->filter);
	}

	conn_fd = kev->ident;

	if (kev->flags & EV_EOF) {
		V("Connection %d dropped due to EOF. ERR: %d\n", conn_fd, kev->fflags);
		drop_conn(tinfo, kev);
		return;
	}

	status = readbuf(conn_fd, data, MESSAGE_LENGTH);

	if (status < 0) {
		W("Connection %d dropped due to readbuf. ERR: %d\n", conn_fd, errno);
		drop_conn(tinfo, kev);
		return;
	}

	
	V("Connection %d sent \"%s\"\n", conn_fd, data);

	if (memcmp(data, MAGIC_STRING, MESSAGE_LENGTH) == 0) {
		
		V("Connection %d asked for QPS - %ld\n", conn_fd, 0l);

		status = writebuf(conn_fd, (void*)&perf_avg, sizeof(long));

		if (status < 0) {
			W("Connection %d dropped due to writebuf. ERR: %d\n", conn_fd, errno);
			drop_conn(tinfo, kev);
		}

		int server_delay = 0;
		int now = get_time_us();
		if (options.delay) {
			server_delay = get_next_delay();
		} else if (options.conn_delay) {
			server_delay = ((struct conn_hint*)(kev->udata))->delay;
		}

		while (get_time_us() - now <= (uint64_t)server_delay) {};

	} else if (memcmp(data, IGNORE_STRING, MESSAGE_LENGTH) == 0){
		/* do nothing */
	} else {
		/* echo back */
		status = writebuf(conn_fd, data, MESSAGE_LENGTH);

		if (status < 0) {
			W("Connection %d dropped due to writebuf. ERR: %d\n", conn_fd, errno);
			drop_conn(tinfo, kev);
		}
	}

	tinfo->evcnt++;
}

static void* 
work_thread(void *info) 
{
	struct worker_thread *tinfo = (struct worker_thread *)info;
	struct kevent kev[NEVENT];
	int status;

	V("Thread %d started.\n", tinfo->id);
	
	while (1) {
		status = kevent(tinfo->kqfd, NULL, 0, kev, NEVENT, NULL);

		if (status < 0) {
			perror("Thread kevent");
			exit(1);
		}

		for (int i = 0; i < status; i++) {
			handle_event(tinfo, &kev[i]);
		}
	}
}

static void
usage() 
{
	fprintf(stdout, "Usage:\n"
	       "    p: server port\n" 
		   "    v: verbose mode\n"
		   "    a: affinitize worker threads\n"
		   "    m: enable SKQ\n"
		   "    d: SKQ dump interval (s)\n"
		   "    t: number of server threads\n"
		   "    h: show help\n"
		   "    D: enable simulated delay, overwrites -c\n"
		   "    c: enable per-connection delay\n");
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
    
    if(cur >= ncpu) {
        cur = cur % ncpu;
    }
    
    ret = cur;
    cur = cur + 2;

    return ret;
}


static void
create_workers(int kq, vector<struct worker_thread*> *thrds)
{
	int status;

	/* Start threads */
	for (int i = 0; i < options.threads; i++) {
		V("Starting worker thread %d...\n", i);

		struct worker_thread *thrd = new worker_thread;
		thrd->evcnt = 0;
		thrd->id = i;
		
		if (!options.skq) {
			kq = kqueue();
			if (kq <= 0) {
				E("Cannot create kqueue\n");
			}
		}

		thrd->kqfd = kq;

		pthread_attr_t  attr;
		pthread_attr_init(&attr);

		if (options.cpu_affinity) {
			int tgt;
			cpuset_t cpuset;

			tgt = get_next_cpu();
	
			V("Setting worker thread's affinity to CPU %d %d\n", tgt, tgt + 1);

			CPU_ZERO(&cpuset);
			CPU_SET(tgt, &cpuset);
			CPU_SET(tgt + 1, &cpuset);

			status = pthread_attr_setaffinity_np(&attr, sizeof(cpuset), &cpuset);

			if (status != 0) {
					E("Can't set affinity: %d\n", status);
			}
		}

		
		status = pthread_create(&thrd->thrd, &attr, work_thread, thrd);

		thrds->push_back(thrd);
	}
}

/*
 * Creates a worker thread.
 */
int 
main(int argc, char *argv[]) 
{
	int kq = -1;
	int status;
	char ch;
	char dbuf[1024 * 1024 + 1];
	vector<struct worker_thread *> wrk_thrds;
    vector<int> server_socks;
	
	while ((ch = getopt(argc, argv, "Dd:p:at:m:vhc")) != -1) {
		switch (ch) {
			case 'D':
				options.delay = 1;
				break;
			case 'd':
				options.skq_dump = atoi(optarg);
				break;
			case 'p':
				options.port = atoi(optarg);
				break;
			case 'a':
				options.cpu_affinity = 1;
				break;
			case 't':
				options.threads = atoi(optarg);
				break;
			case 'm':
				options.skq = 1;
				options.skq_flag = atoi(optarg);
				break;
			case 'v':
				options.verbose = 1;
				W("Verbose mode can cause SUBSTANTIAL latency fluctuations in some terminals!\n");
				break;
			case 'c':
				options.conn_delay = 1;
				break;
			case 'h':
			case '?':
				usage();
				exit(0);
			default:
				E("Unrecognized option -%c. See -h.\n\n", ch);
		}
	}

	// don't raise SIGPIPE when sending into broken TCP connections
	::signal(SIGPIPE, SIG_IGN); 

	V("Setting up listen sockets...\n");

	server_socket_prepare(&server_socks);

	int mkqfd = kqueue();
	struct kevent kev;

	for(uint32_t i = 0; i < server_socks.size(); i++) {
		EV_SET(&kev, server_socks.at(i), EVFILT_READ, EV_ADD, 0, 0, NULL);

		status = kevent(mkqfd, &kev, 1, NULL, 0, NULL);
		if (status == -1) {
			E("Kevent failed %d\n", errno);
		}
	}

#define TIMER_FD (-1234)
	EV_SET(&kev, TIMER_FD, EVFILT_TIMER, EV_ADD, NOTE_MSECONDS, 1, NULL);
	status = kevent(mkqfd, &kev, 1, NULL, 0, NULL);
	if (status == -1) {
		E("Kevent failed %d\n", errno);
	}

	if (options.skq) {
		kq = kqueue();
		status = ioctl(kq, FKQMULTI, &options.skq_flag);
		if (status == -1) {
			E("ioctl failed. ERR %d\n", errno);
		}
		V("SKQ enabled: %d\n", options.skq_flag);
	}

	/* delay distribution table */
	vector<int> dist_table;
	int dist_table_idx = 0;

	if (options.conn_delay) {
		V("Building delay distribution table...\n");
		build_delay_table(&dist_table);
	}

	srand(time(NULL));
	create_workers(kq, &wrk_thrds);
	V("Entering main event loop...\n");
	int cur_thread = 0;
	long cur_ts = 0;
	while (1) {
		status = kevent(mkqfd, NULL, 0, &kev, 1, NULL);
		
		if (status != 1) {
			E("Accept loop kevent failed %d\n", errno);
		}

		if (kev.ident == (uintptr_t)TIMER_FD) {
			/* timer event */
			if (options.skq_dump > 0) {
				if (cur_ts % (options.skq_dump * 1000) == 0) {

					uintptr_t args = (uintptr_t)dbuf;
					memset(dbuf, 0, 1024 * 1024 + 1);
					status = ioctl(kq, FKQMPRNT, &args);
					if (status == -1) {
						E("SKQ dump failed %d\n", errno);
					} else {
						fprintf(stdout, "====== KQ DUMP ======\n%s\n", dbuf);
					}
				}
			}
			
			long tmp_ev = 0;

			/* calculate the average throughput for the past second */
			for (uint32_t i = 0; i < wrk_thrds.size(); i++) {
				tmp_ev += wrk_thrds.at(i)->evcnt;
			}
			perf_sample[cur_ts % 1000] = tmp_ev;
			perf_avg = tmp_ev - perf_sample[(cur_ts + 1) % 1000];
			cur_ts++;
		} else {
			int conn_fd = accept(kev.ident, NULL, NULL);

			if (conn_fd < 0) {
				W("Accept() failed on socket %d\n", (int)kev.ident);
				continue;
			}

			if (setsockopt(conn_fd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable)) < 0) {
				W("setsockopt() failed on socket %d\n", conn_fd);
				continue;
			}

			V("Accepted new connection: %d\n", conn_fd);

			int target_kq = wrk_thrds.at(cur_thread)->kqfd;

			struct conn_hint *hint = new struct conn_hint;

			if (options.conn_delay) {
				hint->delay = dist_table[dist_table_idx % dist_table.size()];
				dist_table_idx++;
				V("Assigned connection %d delay %d us\n", conn_fd, hint->delay);
			}

			EV_SET(&kev, conn_fd, EVFILT_READ, EV_ADD, 0, 0, hint);
			status = kevent(target_kq, &kev, 1, NULL, 0, NULL);

			if (status == -1) {
				E("Accept() kevent failed %d\n", errno);
			}

			V("Connection %d assigned to thread %d\n", conn_fd, wrk_thrds.at(cur_thread)->id);
			cur_thread = (cur_thread + 1) % wrk_thrds.size();
		}		
	}
	
	return 0;
}
