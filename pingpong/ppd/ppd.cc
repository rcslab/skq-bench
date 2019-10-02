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
#include <set>
#include <netdb.h>
#include <sstream>

#include "../common.h"

using namespace std;

#define NEVENT (256)
#define SOCK_BACKLOG (10000)
#define SINGLE_LEGACY (-1)

struct server_option {
	int threads;
	int port;
	int skq;
	int skq_flag;
	int cpu_affinity;
	int skq_dump;
	int verbose;
	
	/* only applies to ECHO requests */
	int delay;
	int conn_delay;

	vector<char*> hpip;
	int kq_rtshare;
	int kq_tfreq;
	
	/* only applies to TOUCH requests */
	int table_sz;
};

struct cache_item {
	int val;
	char _pad[CACHE_LINE_SIZE - sizeof(int)];
};
_Static_assert(sizeof(struct cache_item) == CACHE_LINE_SIZE, "cache_item not cache line sized");

struct conn_hint {
	int delay;
	struct cache_item *items;
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
								.conn_delay = 0,
								.kq_rtshare = 100,
								.kq_tfreq = 0,
								.table_sz = 64};

/* XXX: legacy stuff */
static const int enable = 1;

/* cpu assignment */
static int ncpu = 0;
static int cur = 0;

static void
conn_hint_destroy(struct conn_hint *hint)
{
	if (hint->items != NULL) {
		free(hint->items);
	}
	delete hint;
}

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

	conn_hint_destroy((struct conn_hint *)kev->udata);
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

static const struct ppd_msg GENERIC_REPLY = { .cmd = PPD_CRESP,
									.dat_len = 0 };

static int
handle_event(struct worker_thread *tinfo, struct kevent* kev)
{
	int conn_fd;
	int status;
	struct conn_hint *hint;
	struct ppd_msg *msg;

	if (kev->filter != EVFILT_READ) {
		E("Unknown event filter %d\n", kev->filter);
	}

	conn_fd = kev->ident;
	hint = (struct conn_hint*)kev->udata;

	if (kev->flags & EV_EOF) {
		V("Connection %d dropped due to EOF. ERR: %d\n", conn_fd, kev->fflags);
		drop_conn(tinfo, kev);
		return ECONNRESET;
	}

	status = read_msg(conn_fd, &msg);

	if (status < 0) {
		W("Connection %d dropped due to readbuf. ERR: %d\n", conn_fd, errno);
		drop_conn(tinfo, kev);
		return ECONNRESET;
	}

	V("Connection %d cmd %d, dat_len %d \n", conn_fd, msg->cmd, msg->dat_len);

	switch (msg->cmd) {
		case PPD_CECHO: {
			if (msg->dat_len != sizeof(struct ppd_echo_arg)) {
				W("Connection %d invalid echo request\n", conn_fd);
				break;
			}

			/* XXX: not cross-endianess */
			struct ppd_echo_arg *arg = (struct ppd_echo_arg*)&msg->dat[0];

			if (arg->enable_delay) {
				uint64_t server_delay = 0;
				uint64_t now = get_time_us();
				if (options.delay) {
					server_delay = get_next_delay();
				} else if (options.conn_delay) {
					server_delay = hint->delay;
				}

				V("Conn %d Thread %d delaying for %ld...\n", conn_fd, tinfo->id, server_delay);
				while (get_time_us() - now <= server_delay) {};
			}

			status = write_msg(conn_fd, &GENERIC_REPLY);

			if (status < 0) {
				W("Connection %d dropped due to writebuf. ERR: %d\n", conn_fd, errno);
				drop_conn(tinfo, kev);
			}
			break;
		}
		case PPD_CTOUCH: {
			if (msg->dat_len != sizeof(struct ppd_touch_arg)) {
				W("Connection %d invalid touch request\n", conn_fd);
				break;
			}



			/* XXX: not cross-endianess */
			struct ppd_touch_arg *arg = (struct ppd_touch_arg*)&msg->dat[0];

			if (hint->items != NULL) {
				V("Conn %d Thread %d updating %d items val %d...\n", conn_fd, tinfo->id, arg->touch_cnt, arg->inc);
				int sum;
				for(uint32_t i = 0; i < arg->touch_cnt; i++) {
					if (arg->inc > 0) {
						hint->items[i % options.table_sz].val += arg->inc;
					} else {
						/* only read */
						sum = hint->items[i % options.table_sz].val;
					}	
				}
			}

			status = write_msg(conn_fd, &GENERIC_REPLY);

			if (status < 0) {
				W("Connection %d dropped due to writebuf. ERR: %d\n", conn_fd, errno);
				drop_conn(tinfo, kev);
			}
			break;
		}
		default: {
			W("Received unknown cmd %d from conn %d", msg->cmd, conn_fd);
			break;
		}
	}

	free(msg);
	tinfo->evcnt++;
	return 0;
}

static void* 
work_thread(void *info) 
{
	struct worker_thread *tinfo = (struct worker_thread *)info;
	struct kevent kev[NEVENT];
	struct kevent skev[NEVENT];
	int skev_sz;
	int status;

	V("Thread %d started.\n", tinfo->id);
	
	skev_sz = 0;
	while (1) {
		status = kevent(tinfo->kqfd, skev, skev_sz, kev, NEVENT, NULL);

		if (status <= 0) {
			perror("Thread kevent");
			exit(1);
		}

		skev_sz = 0;
		for (int i = 0; i < status; i++) {
			if (handle_event(tinfo, &kev[i]) == 0) {
				if (options.skq && options.skq_flag == SINGLE_LEGACY) {
					EV_SET(&skev[skev_sz], (int)kev[i].ident, EVFILT_READ, EV_ENABLE, 0, 0, kev[i].udata);
					skev_sz++;
					V("Thread %d queued connection %d for EV_ENABLE\n", tinfo->id, (int)kev[i].ident);
				}
			}
		}
	}
}

void dump_options()
{
	stringstream ss;
	ss << "Configuration:\n"
	   << "        worker threads: " << options.threads << endl
	   << "        port: " << options.port << endl
	   << "        single KQ: " << options.skq << endl
	   << "        single KQ flags: " << options.skq_flag << endl
	   << "        single KQ dump:" << options.skq_dump << endl
	   << "        CPU affinity: " << options.cpu_affinity << endl
	   << "        verbose: " << options.verbose << endl
	   << "        generic delay: " << options.delay << endl
	   << "        connection delay: " << options.conn_delay << endl
	   << "        kq_rtshare: " << options.kq_rtshare << endl
	   << "        kq_tfreq: " << options.kq_tfreq << endl
	   << "        connection item size: " << options.table_sz << endl
	   << "        priority client IPs (" << options.hpip.size() << "): " << endl;

	for(uint i = 0; i < options.hpip.size(); i++) {
		ss << "                " << options.hpip.at(i) << endl;
	}

	V("%s", ss.str().c_str());
}

static void
usage() 
{
	fprintf(stdout, "Usage:\n"
	       "    p: server port\n" 
		   "    v: verbose mode\n"
		   "    a: affinitize worker threads\n"
		   "    m: use a single KQ (SKQ and legacy)\n"
		   "    d: SKQ dump interval (s)\n"
		   "    t: number of server threads\n"
		   "    h: show help\n"
		   "    D: enable simulated delay, overwrites -c\n"
		   "    c: enable per-connection delay\n"
		   "    r: realtime client hostname\n"
		   "    R: realtime share\n"
		   "    F: kqueue frequency\n"
		   "    M: per connection table entry count. Default 64.\n");
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
		V("Creating worker thread %d...\n", i);

		struct worker_thread *thrd = new worker_thread;
		thrd->evcnt = 0;
		thrd->id = i;
		
		if (!options.skq) {
			kq = kqueue();
			if (kq <= 0) {
				E("Cannot create kqueue\n");
			}

#ifdef FKQMULTI
			int para = KQTUNE_MAKE(KQTUNE_RTSHARE, options.kq_rtshare);
			status = ioctl(kq, FKQTUNE, &para);
			if (status == -1) {
				E("rtshare ioctl failed. ERR %d\n", errno);
			}
			
			para = KQTUNE_MAKE(KQTUNE_FREQ, options.kq_tfreq);
			status = ioctl(kq, FKQTUNE, &para);
			if (status == -1) {
				E("freq ioctl failed. ERR %d\n", errno);
			}
#endif

			V("Thread %d KQ created. RTSHARE %d TFREQ %d\n", i, options.kq_rtshare, options.kq_tfreq);
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

static void
get_ip_from_hostname(const char* hostname, char* buf, int len) 
{
  struct in_addr **addr;
  struct hostent *he;

  if ((he = gethostbyname(hostname)) == NULL) {
    fprintf(stderr, "Hostname %s cannot be resolved.\n", hostname);
    exit(1);
  }
  addr = (struct in_addr**)he->h_addr_list;
  for (int i=0;addr[i]!=NULL;i++) {
    strncpy(buf, inet_ntoa(*addr[i]), len);
    break;
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
	vector<struct worker_thread *> wrk_thrds;
    vector<int> server_socks;

#ifdef FKQMULTI
	char dbuf[1024 * 1024 + 1];
#endif
	
	while ((ch = getopt(argc, argv, "Dd:p:at:m:vhcr:R:F:M:")) != -1) {
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
			case 'M':
				options.table_sz = atoi(optarg);
				break;
			case 'v':
				options.verbose = 1;
				W("Verbose mode can cause SUBSTANTIAL latency fluctuations in some terminals!\n");
				break;
			case 'c':
				options.conn_delay = 1;
				break;
			case 'r': {
				char* eip = new char[INET_ADDRSTRLEN + 1];
				get_ip_from_hostname(optarg, eip, INET_ADDRSTRLEN);
				options.hpip.push_back(eip);
				break;
			}
			case 'R':
				options.kq_rtshare = atoi(optarg);
				break;
			case 'F':
				options.kq_tfreq = atoi(optarg);
				break;
			case 'h':
			case '?':
				usage();
				exit(0);
			default:
				E("Unrecognized option -%c. See -h.\n\n", ch);
		}
	}

	dump_options();

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

		if (kq <= 0) {
			E("Failed to create kqueue. ERR %d\n", errno);
		}

#ifdef FKQMULTI
		if (options.skq_flag != SINGLE_LEGACY) {
			status = ioctl(kq, FKQMULTI, &options.skq_flag);
			if (status == -1) {
				E("ioctl failed. ERR %d\n", errno);
			}
			V("SKQ enabled. SFLAG %d\n", options.skq_flag);
		}

		int para = KQTUNE_MAKE(KQTUNE_RTSHARE, options.kq_rtshare);
		status = ioctl(kq, FKQTUNE, &para);
		if (status == -1) {
			E("rtshare ioctl failed. ERR %d\n", errno);
		}

		para = KQTUNE_MAKE(KQTUNE_FREQ, options.kq_tfreq);
		status = ioctl(kq, FKQTUNE, &para);
		if (status == -1) {
			E("freq ioctl failed. ERR %d\n", errno);
		}
		V("KQ IOCTL: RTSHARE %d TFREQ %d\n", options.kq_rtshare, options.kq_tfreq);
#else 
		/* legacy single KQ only supports flag -1 */
		options.skq_flag = SINGLE_LEGACY;
#endif
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
		if (kevent(mkqfd, NULL, 0, &kev, 1, NULL) != 1) {
			E("Accept loop kevent failed %d\n", errno);
		}

		if (kev.ident == (uintptr_t)TIMER_FD) {
#ifdef FKQMULTI
			/* timer event */
			if (options.skq_dump > 0 && cur_ts % (options.skq_dump * 1000) == 0) {
				uintptr_t args = (uintptr_t)dbuf;
				memset(dbuf, 0, 1024 * 1024 + 1);
				status = ioctl(kq, FKQMPRNT, &args);
				if (status == -1) {
					E("SKQ dump failed %d\n", errno);
				} else {
					fprintf(stdout, "====== KQ DUMP ======\n%s\n", dbuf);
				}
			}
#endif
			cur_ts++;
			continue;
		}

		struct sockaddr_in client_addr;
		socklen_t client_addr_size = sizeof(client_addr);

		int conn_fd = accept(kev.ident, (struct sockaddr*)&client_addr, &client_addr_size);

		if (conn_fd < 0) {
			W("Accept() failed on socket %d\n", (int)kev.ident);
			continue;
		}

		if (setsockopt(conn_fd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable)) < 0) {
			W("setsockopt() failed on socket %d\n", conn_fd);
			continue;
		}

		char ipaddr[INET_ADDRSTRLEN + 1];
		strncpy(ipaddr, inet_ntoa(client_addr.sin_addr), INET_ADDRSTRLEN);
		ipaddr[INET_ADDRSTRLEN] = 0;

		V("Accepted new connection %d from %s\n", conn_fd, ipaddr);

		struct conn_hint *hint = new struct conn_hint;

		if (options.table_sz > 0) {
			V("Allocating %d items x %d CASZ for connnection %d\n", options.table_sz, CACHE_LINE_SIZE, conn_fd);
			hint->items = (struct cache_item *)aligned_alloc(CACHE_LINE_SIZE, options.table_sz * sizeof(struct cache_item));
			if (hint->items == NULL) {
				W("Connection %d dropped - failed to allocate memory for items\n", conn_fd);
				close(conn_fd);
				continue;
			}
		} else {
			hint->items = NULL;
		}

		int target_kq = wrk_thrds.at(cur_thread)->kqfd;

		if (options.conn_delay) {
			hint->delay = dist_table[dist_table_idx % dist_table.size()];
			dist_table_idx++;
			V("Assigned connection %d delay %d us\n", conn_fd, hint->delay);
		}

		int ev_flags = EV_ADD;

#ifdef FKQMULTI
		for (uint32_t i = 0; i < options.hpip.size(); i++) {
			if (strcmp(ipaddr, options.hpip.at(i)) == 0) {
				V("Connection %d marked as realtime.\n", conn_fd);
				ev_flags |= EV_REALTIME;
			}
		}
#endif

		if (options.skq && options.skq_flag == SINGLE_LEGACY) {
			ev_flags |= EV_DISPATCH;
		}

		EV_SET(&kev, conn_fd, EVFILT_READ, ev_flags, 0, 0, hint);
		status = kevent(target_kq, &kev, 1, NULL, 0, NULL);

		if (status == -1) {
			E("Accept() kevent failed %d\n", errno);
		}

		V("Connection %d assigned to thread %d\n", conn_fd, wrk_thrds.at(cur_thread)->id);
		cur_thread = (cur_thread + 1) % wrk_thrds.size();
	}
	
	return 0;
}
