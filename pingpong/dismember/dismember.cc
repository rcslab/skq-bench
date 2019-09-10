#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/event.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <signal.h>
#include <err.h>
#include <netdb.h>

#include <iostream>
#include <vector>
#include <thread>
#include <set>

#include "../common.h"

using namespace std;

struct option {
	int verbose;

	const char *output_name;

	int client_num;
	int client_thread_count;
	int master_thread_count;

	int client_conn;
	int master_conn;

	int target_qps;
	int master_qps;

	int master_mode;
	int client_mode;

	char server_ip[33];
	int server_port;
	int depth_limit = 1;

	int warmup;
	int duration;
};

static const char * DEFAULT_OUTPUT = "output.sample";

static struct option options = {.verbose = 0,
						 .server_ip = {0},
						 .output_name = DEFAULT_OUTPUT,
						 .client_thread_count = 1,
						 .master_thread_count = -1,
						 .client_conn = 1,
						 .master_conn = -1,
						 .target_qps = 1,
						 .master_qps = -1,
						 .client_mode = 0,
						 .master_mode = 0,
						 .warmup = 0,
						 .duration = 10,
						 .server_port = DEFAULT_SERVER_CLIENT_CONN_PORT};

/* client server stuff */
static vector<char *> client_ips;
static vector<int> client_fds;
static int master_fd = 0;

static int mt_kq; /* kq for the main thread. Has all connections and the clock timer */

static pthread_barrier_t prepare_barrier;
static pthread_barrier_t ok_barrier;

static const int depth_limit = 1;
static volatile uint64_t cur_time = 0;

struct datapt {
	uint64_t qps;
	uint64_t lat;
};

enum conn_state {
	STATE_LIMITING,
	STATE_WAITING,
	STATE_SENDING
};

struct kqconn{
	int conn_fd;
	int timer;
	int timer_expired;
	int kq;
	int depth;
	enum conn_state state;
	uint64_t next_send;
	uint64_t last_send;
	vector<struct kqconn*> *conns;
	vector<struct datapt*> *stats;
};

void kqconn_cleanup(struct kqconn *conn)
{
	int status;
	struct kevent ev[2];
	int nev = 1;
	EV_SET(&ev[0], conn->conn_fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
	if (!conn->timer_expired) {
		EV_SET(&ev[1], conn->timer, EVFILT_TIMER, EV_DELETE, 0, 0, NULL);
		nev++;
	}
	
	status = kevent(conn->kq, ev, nev, NULL, 0, NULL);
	if (status == -1) {
		E("Error kevent kqconn_cleanup\n");
	}
}

void kqconn_state_machine(struct kqconn *conn)
{
	int64_t now;
	struct kevent ev;
	while(1) {
		switch (conn->state) {
			case STATE_WAITING: {
				now = conn->next_send - (int64_t)get_time_us();
				if (now > 0) {
					if (conn->timer_expired) {
						EV_SET(&ev, conn->timer, EVFILT_TIMER, EV_ADD | EV_ONESHOT, NOTE_USECONDS, now, conn);
						if (kevent(conn->kq, &ev, 1, NULL, 0, NULL) == -1) {
							E("Error arming timer %d. ERR %d\n", conn->timer, errno);
						}
						conn->timer_expired = 0;
					}
					return;
				} else {
					conn->state = STATE_SENDING;
					break;
				}
			}

			case STATE_LIMITING:
				if (conn->depth >= depth_limit) {
					return;
				}
				conn->state = STATE_WAITING;
				break;

			case STATE_SENDING: {
				/* send one packet and transfer to wait state */
				if (conn->depth >= depth_limit) {
					conn->state = STATE_LIMITING;
					/* wait for the read to call us */
					break;
				} else {
					now = get_time_us();
					if (now < (int64_t)conn->next_send) {
						/* only STATE_WAITING transfers us to this state
						 * the above condition cannot be true
						 */
						E("Oscar fucked up\n");
					}

					//V("Packet sent in conn %d\n", conn->conn_fd);

					conn->depth++;
					conn->next_send += 1000 * 1000 / ((options.target_qps / options.client_thread_count) / conn->conns->size());
					conn->last_send = now;
					conn->state = STATE_WAITING;

					if (writebuf(conn->conn_fd, MAGIC_STRING, MESSAGE_LENGTH) < 0) {
						/* effectively skips this packet */
						W("Cannot write to connection %d\n", conn->conn_fd);
					}
					break;
				}
			}

			default:
				E("Oscar fucked up hard\n");
		}
	}
}

void ev_loop(struct kevent *ev, struct kqconn *conn)
{
	int status;
	long q;

	if ((int)ev->ident == conn->conn_fd) {
		/* we got something to read */
		status = readbuf(conn->conn_fd, &q, sizeof(long));
		if (status < 0) {
			E("Connection %d readbuf failed. ERR: %d\n", conn->conn_fd, errno);
		}

		conn->depth--;
		if (conn->depth < 0) {
			E("More recved packets than sent.\n");
		}
		

		if (!options.client_mode && (long)cur_time > options.warmup) {
			struct datapt* dat = new struct datapt;
			dat->qps = q;
			dat->lat = get_time_us() - conn->last_send;
			conn->stats->push_back(dat);
			V("Conn %d: TS: %d LAT: %ld, QPS: %ld\n", conn->conn_fd, (int)cur_time, dat->lat, (long)dat->qps);
		}

		kqconn_state_machine(conn);		

	} else if ((int)ev->ident == conn->timer) {
		conn->timer_expired = 1;
		kqconn_state_machine(conn);
	} else {
		E("Oscar really fucked up hard\n");
	}
}

void 
worker_thread(int id, int notif_pipe, vector<struct datapt*> *data)
{
	int timer_start = -1;
	int conn_fd;
	struct sockaddr_in server_addr;
	vector<struct kqconn*> conns;
	struct kevent ev[2];
	int kq = kqueue();

	EV_SET(&ev[0], notif_pipe, EVFILT_READ, EV_ADD, 0, 0, NULL);
	if (kevent(kq, &ev[0], 1, NULL, 0, NULL) == -1) {
		E("conn fd event kq reg problem");
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(options.server_port);
	server_addr.sin_addr.s_addr = inet_addr(options.server_ip);
	
	// initialize all the connections
	for (int i = 0 ; i < options.client_conn; i++) {
		while (true) {
			int enable = 1;
			struct timeval tv = { .tv_sec = 5, tv.tv_usec = 0 };

			conn_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (conn_fd == -1) {
				W("Error in creating socket, will retry.\n");
				continue;
			}
			if (setsockopt(conn_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0) {
				E("setsockopt rcvtimeo");
			}	
			if (setsockopt(conn_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
				E("setsockopt reuseaddr");
			}
			if (setsockopt(conn_fd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) < 0) {
				E("setsockopt reuseport");
			}
			if (connect(conn_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
				E("Connection %d connect() failed. Dropping. Err: %d\n", conn_fd, errno);
			}
			if (writebuf(conn_fd, IGNORE_STRING, MESSAGE_LENGTH) < 0) {
				E("Connection %d handshake failed. Dropping. Err: %d\n", conn_fd, errno);
			}

			struct kqconn* english = new struct kqconn;

			english->conn_fd = conn_fd;
			english->timer = timer_start--;
			english->kq = kq;
			english->depth = 0;
			english->conns = &conns;
			english->next_send = 0;
			english->timer_expired = 0;
			english->state = STATE_WAITING;
			english->stats = data;

			EV_SET(&ev[0], english->conn_fd, EVFILT_READ, EV_ADD, 0, 0, english);
			EV_SET(&ev[1], english->timer, EVFILT_TIMER, EV_ADD | EV_ONESHOT, NOTE_USECONDS, 0, english);
			if (kevent(kq, ev, 2, NULL, 0, NULL) == -1) {
				E("conn fd event kq reg problem");
			}

			conns.push_back(english);
			usleep(50);
			break;
		}
	}

	V("Thread %d has established %ld connections.\n", id, conns.size());
	pthread_barrier_wait(&prepare_barrier);

	V("Thread %d waiting for START...\n", id);
	pthread_barrier_wait(&ok_barrier);

	V("Thread %d running...\n", id);
	/* send the initial packet now */
	for (uint32_t i = 0; i < conns.size(); i++) {
		conns.at(i)->next_send = get_time_us();
	}

	while(1) {
		struct kevent ev;
		if (kevent(kq, NULL, 0, &ev, 1, NULL) == 1) {
			struct kqconn *conn = (struct kqconn *)ev.udata;

			if (ev.flags & EV_EOF) {
				W("Connection %d dropped due to EOF. ERR: %d\n", conn->conn_fd, ev.fflags);
				vector<struct kqconn*>::iterator pos = find(conns.begin(), conns.end(), conn);
				conns.erase(pos);
				kqconn_cleanup(conn);
				delete conn;
				continue;
			}

			if ((int)ev.ident == notif_pipe) {
				char c;

				if (read(notif_pipe, &c, sizeof(c)) == -1)
					E("Error reading pipe. ERR %d\n", errno);

				if (c == 'e')
					break;
			}

			ev_loop(&ev, conn);
		} else {
			E("Thread %d kevent failed. ERR %d\n", id, errno);
		}
	}

	for (uint32_t i = 0; i < conns.size(); i++) {
		kqconn_cleanup(conns.at(i));
		delete conns.at(i);
	}

	close(kq);

	V("Thread %d exiting...\n", id);
}

void 
client_stop()
{
	int msg = MSG_TEST_STOP;
	int stop_cnt = 0;
	struct kevent kev;

	V("Sending clients STOP...\n");
	for (uint32_t i=0;i<client_fds.size();i++) {
		if (writebuf(client_fds[i], &msg, sizeof(msg)) < 0) {
			E("Failed to send client STOP. ERR %d\n", errno);
		}
	}

	V("Waiting for client connections to close.\n");
	while (stop_cnt < (int)client_fds.size()) {
		if (kevent(mt_kq, NULL, 0, &kev, 1, NULL) == -1) {
			E("Error waiting for clients to exit. ERR %d\n", errno);
		}

		if (kev.flags & EV_EOF) {
			V("Client %d disconnected.\n", (int)kev.ident);
			stop_cnt++;
			close(kev.ident);
		}
	}

	for (uint32_t i = 0; i < client_ips.size(); i++) {
		delete[] client_ips.at(i);
	}
}

static std::string 
get_ip_from_hostname(std::string hostname) 
{
  static char rt[100] = {0};
  struct in_addr **addr;
  struct hostent *he;

  if ((he = gethostbyname(hostname.c_str())) == NULL) {
    E("Hostname %s cannot be resolved.\n", hostname.c_str());
  }
  addr = (struct in_addr**)he->h_addr_list;
  for (int i=0;addr[i]!=NULL;i++) {
    strncpy(rt, inet_ntoa(*addr[i]), 99);
    return rt;
  }
  return rt;
}

static void send_master_ok()
{
	struct kevent kev;
	int status;
	int msg = MSG_TEST_OK;
	status = writebuf(master_fd, &msg, sizeof(msg));

	if (status < 0) {
		E("Failed to respond OK to master. ERR %d\n", errno);
	}

	/* wait for START */
	status = kevent(mt_kq, NULL, 0, &kev, 1, NULL);

	if (status != 1) {
		E("Failed to wait for START. ERR %d\n", errno);
	}

	status = readbuf(master_fd, &msg, sizeof(msg));
	
	if (status == -1 || msg != MSG_TEST_START) {
		E("Failed to read START from master. ERR %d\n", errno);
	}
}

static void wait_clients_ok()
{
	set<int> acked;
	int status;
	int msg;
	struct kevent kev;

	/* wait for client OKs */
	while(acked.size() < client_fds.size()) {
		status = kevent(mt_kq, NULL, 0, &kev, 1, NULL);

		if (status != 1) {
			E("kevent wait for client ok %d\n", errno);
		}

		V("Received client ok from %d\n", (int)kev.ident);

		status = readbuf(kev.ident, &msg, sizeof(int));

		if (status < 0) {
			E("readbuf wait for client ok %d\n", errno);
		}

		if (msg != MSG_TEST_OK || acked.find(kev.ident) != acked.end()) {
			E("Duplicate or invalid client ok message\n");
		}

		acked.insert(kev.ident);
	}

	/* start all clients */
	msg = MSG_TEST_START;
	for (uint32_t i = 0; i < client_fds.size(); i++) {
		if(writebuf(client_fds.at(i), &msg, sizeof(int)) < 0) {
			E("Error sending START to client %d. ERR %d\n",client_fds.at(i) , errno);
		}
	}
}

static void prepare_clients()
{
	int real_conn = options.master_conn == -1 ? options.client_conn : options.master_conn;
	int real_thread = options.master_thread_count == -1 ? options.client_thread_count : options.master_thread_count;
	int real_qps;
	
	if (options.master_qps != -1) {
		real_qps = options.master_qps;
	} else {
		real_qps = options.target_qps * (real_conn * real_thread) / (real_conn * real_thread + options.client_num * options.client_conn * options.client_thread_count);
	}

	/* this is the qps for each client */
	options.target_qps = (options.target_qps - real_qps) / client_ips.size();

	struct kevent *kev = new struct kevent[client_ips.size()];

	/* create a connection to the clients */
	for (uint32_t i=0; i < client_ips.size(); i++) {
		struct sockaddr_in csock_addr;
		int c_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		csock_addr.sin_family = AF_INET;
		csock_addr.sin_addr.s_addr = inet_addr(client_ips[i]);
		csock_addr.sin_port = htons(DEFAULT_CLIENT_CTL_PORT);

		V("Connecting to client %s...\n", client_ips.at(i));

		if (connect(c_fd, (struct sockaddr*)&csock_addr, sizeof(csock_addr)) != 0) {
			E("Connect failed. ERR %d\n", errno);
		}

		if (writebuf(c_fd, &options, sizeof(options)) < 0) {
			E("Write to client. ERR %d\n", errno);
		}

		client_fds.push_back(c_fd);
		V("Client connected %d/%lu.\n", i + 1, client_ips.size());

		EV_SET(&kev[i], c_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
	}

	V("Registering client fds to mtkq...\n");
	/* add to main thread's kq */
	if (kevent(mt_kq, kev, client_ips.size(), NULL, 0, NULL) == -1) {
		E("Failed to add some clients to mtkq. ERR %d\n", errno);
	}

	delete[] kev;

	/* adjust the existing settings */
	options.client_conn = real_conn;
	options.client_thread_count = real_thread;
	options.target_qps = real_qps;
}

static void wait_master_prepare()
{
	V("Waiting for master's PREPARE...\n");

	struct sockaddr_in csock_addr;
	int listen_fd;
	int enable = 1;
	csock_addr.sin_family = AF_INET;
	csock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	csock_addr.sin_port = htons(DEFAULT_CLIENT_CTL_PORT);
	listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (listen_fd < 0) {
		E("socket");
	}

	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
		E("setsockopt reuseaddr");
	}
	
	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) < 0) {
		E("setsockopt reuseport");
	}

	if (::bind(listen_fd, (struct sockaddr*)&csock_addr, sizeof(csock_addr)) < 0) {
		E("bind");
	}

	if (listen(listen_fd, 10) < 0) {
		E("ctl listen");
	}

	master_fd = accept(listen_fd, NULL, NULL);
	if (master_fd == -1) {
		E("Failed to accept master. ERR %d\n", errno);
	}

	close(listen_fd);

	if (readbuf(master_fd, &options, sizeof(options)) < 0) {
		E("Failed to receive options from master. ERR %d\n", errno);
	}

	V("Registering master fd to mtkq...\n");
	struct kevent kev;

	EV_SET(&kev, master_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);

	if (kevent(mt_kq, &kev, 1, NULL, 0, NULL) == -1) {
		E("Failed to register master fd to mtkq. ERR %d\n", errno);
	}

	/* set the correct mode */
	options.master_mode = 0;
	options.client_mode = 1;
	options.output_name = NULL;
}

void dump_options()
{
	if (options.verbose) {
		fprintf(stdout,"Configuration:\n"
			"        Connections per thread: %d\n"
			"        Num threads: %d\n"
			"        Target QPS: %d\n"
			"        warmup: %d\n"
			"        duration: %d\n"
			"        master_mode: %d\n"
			"        client_mode: %d\n"
			"        output_file: %s\n"
			"        server_ip: %s\n"
			"        server_port: %d\n",
			options.client_conn,
			options.client_thread_count,
			options.target_qps,
			options.warmup,
			options.duration,
			options.master_mode,
			options.client_mode,
			options.output_name,
			options.server_ip,
			options.server_port);
	}
}

static void usage() 
{
	fprintf(stdout, "Usage:\n"
					"    -s: server addr.\n"
					"    -p: server port.\n"
					"    -q: target qps.\n"
					"    -t: threads per client.\n"
					"    -c: connections per thread.\n"
					"    -o: test output file name.\n"
					"    -h: show help.\n"
					"    -v: verbose mode.\n"
					"    -W: warm up time.\n"
					"    -w: test duration.\n\n"
					"Master mode:\n"
					"    -a: client addr.\n"
					"    -A: client mode.\n"
					"    -C: master connections.\n"
					"    -Q: master qps.\n"
					"    -T: master threads.\n" );
}

/*
 * protocol:
 * 
 * master -> client options
 * client and master all establish connections to the server
 * client -> master OK
 * master -> client START (client runs forever)
 * master RUNS for X seconds
 * master -> client STOP
 */
int
main(int argc, char* argv[]) 
{
	int ch;
	FILE *resp_fp_csv;

	while ((ch = getopt(argc, argv, "q:s:C:p:o:t:c:hvW:w:T:Aa:Q:")) != -1) {
		switch (ch) {
			case 'q':
				options.target_qps = atoi(optarg);
				if (options.target_qps < 0) {
					E("Target QPS must be positive\n");
				}
				break;
			case 'Q':
				options.master_qps = atoi(optarg);
				if (options.master_qps < 0) {
					E("Master QPS must be positive\n");
				}
				break;
			case 's': {
				string ip = get_ip_from_hostname(optarg);
				strncpy(options.server_ip, ip.c_str(), 32);
				options.server_ip[32] = 0;
				break;
			}
			case 'p':
				options.server_port = atoi(optarg);
				if (options.server_port <= 0) {
					E("Server port must be positive\n");
				}
				break;
			case 'o':
				options.output_name = optarg;
				break;
			case 't':
				options.client_thread_count = atoi(optarg);
				if (options.client_thread_count <= 0) {
					E("Client threads must be positive\n");
				}
				break;
			case 'T':
				options.master_thread_count = atoi(optarg);
				if (options.master_thread_count <= 0) {
					E("Master threads must be positive\n");
				}
				break;
			case 'c':
				options.client_conn = atoi(optarg);
				if (options.client_conn <= 0) {
					E("Client connections must be positive\n");
				}
				break;
			case 'C':
				options.master_conn = atoi(optarg);
				if (options.master_conn <= 0) {
					E("Master connections must be positive\n");
				}
				break;
			case 'h':
			case '?':
				usage();
				exit(0);
			case 'v':
				options.verbose = 1;
				W("Verbose mode can cause SUBSTANTIAL latency fluctuations in some terminals! The program will continue in 3 seconds.\n");
				sleep(3);
				break;
			case 'a': {
				if (options.client_mode == 1) {
					E("Cannot be both master and client\n");
				}
				string ip = get_ip_from_hostname(optarg);
				char *rip = new char[33];
				strncpy(rip, ip.c_str(), 32);
				rip[33] = 0;
				client_ips.push_back(rip);
				options.master_mode = 1;
				break;
			}
			case 'W':
				options.warmup = atoi(optarg);
				if (options.warmup < 0) {
					E("Warmup must be positive\n");
				}
				break;
			case 'w':
				options.duration = atoi(optarg);
				if (options.duration <= 0) {
					E("Test duration must be positive\n");
				}
				break;
			case 'A': {
				if (options.master_mode == 1) {
					E("Cannot be both master and client\n");
				}
				options.client_mode = 1;
				break;
			}
			default:
				E("Unrecognized option -%c\n\n", ch);
		}
	}

	::signal(SIGPIPE, SIG_IGN);

	if (!options.client_mode) {
		resp_fp_csv = fopen(options.output_name, "w");
		if (resp_fp_csv == NULL) {
			E("cannot open file for writing %s.", options.output_name);
			exit(1);
		}
	}

	options.client_num = client_ips.size();

	mt_kq = kqueue();

	/* connect to clients and sync options */
	if (options.master_mode) {
		/* make master connections to clients */
		prepare_clients();
	} else if (options.client_mode) {
		/* in client mode we receive all parameters from the server */
		wait_master_prepare();
	}

	dump_options();

	vector<std::thread> threads;
	vector<int> send_pipes;
	vector<int> recv_pipes;
	send_pipes.resize(options.client_thread_count);
	recv_pipes.resize(options.client_thread_count);

	/* do our setup according to the options */
	pthread_barrier_init(&prepare_barrier, NULL, options.client_thread_count + 1);
	pthread_barrier_init(&ok_barrier, NULL, options.client_thread_count + 1);

	V("Creating %d threads...\n", options.client_thread_count);

	/* create worker threads */
    vector<struct datapt *> *data = new vector<struct datapt*>[options.client_thread_count];
	for (int i = 0; i < options.client_thread_count; i++) {
		int pipes[2];
		if (pipe(&pipes[0]) == -1) {
			E("Cannot create pipes. ERR %d\n", errno);
		}
		send_pipes[i] = pipes[0];
		recv_pipes[i] = pipes[1];
		threads.push_back(thread(worker_thread, i, pipes[1], &data[i]));
	}

	V("Waiting for thread connection establishment.\n");
	pthread_barrier_wait(&prepare_barrier);

	if (options.master_mode) {
		V("Waiting for clients...\n");
		/* wait for ok messages from the clients */
		wait_clients_ok();
	} else if (options.client_mode) {
		V("Sending OK to master...\n");
		/* send ok messages to the master */
		send_master_ok();
	}


	/* create main thread timer loop */
	struct kevent kev;
#define TIMER_FD (-1)
	EV_SET(&kev, TIMER_FD, EVFILT_TIMER, EV_ADD, NOTE_SECONDS, 1, NULL);

	if (kevent(mt_kq, &kev, 1, NULL, 0, NULL) == -1) {
		E("Cannot create kevent timer. ERR %d\n", errno);
	}

	/* kick off worker threads */
	pthread_barrier_wait(&ok_barrier);
	/* now we are free to start the experiment */
	V("Main thread running.\n");

	while(1) {
		/* client mode runs forever unless server sends us */
		if ((int)cur_time >= options.duration + options.warmup && !options.client_mode) {
			break;
		}

		if (kevent(mt_kq, NULL, 0, &kev, 1, NULL) != 1) {
			E("Error in main event loop. ERR %d\n", errno);
		}

		if ((int)kev.ident == TIMER_FD) {
			cur_time++;
		} else {
			/* its from either master or client */
			if (kev.flags & EV_EOF) {
				E("Client or master %d disconnected\n", (int)kev.ident);
			}

			int msg;

			if (readbuf(kev.ident, &msg, sizeof(msg)) == -1)
				E("Failed to read from master_fd or message invalid. ERR %d\n", errno);

			if (msg == MSG_TEST_STOP) {
				V("Received STOP from master\n");
				break;
			} else {
				E("Unexpected message from master: %d\n", msg);
			}
		}
	}

	V("Signaling threads to exit...\n");
	for (int i = 0; i < options.client_thread_count; i++) {
		if (write(send_pipes[i], "e", sizeof(char)) == -1) {
			E("Couldn't write to thread pipe %d. ERR %d\n", send_pipes[i], errno);
		}
	}

	for (int i = 0; i < options.client_thread_count; i++) {
		threads.at(i).join();
		close(send_pipes[i]);
		close(recv_pipes[i]);
	}

	if (options.master_mode == 1) {
		V("Shutting down clients...\n");
		client_stop();
	}

	if (!options.client_mode) {
		/* stop the measurement */
		V("Saving results...\n");
		for (int i = 0; i < options.client_thread_count; i++) {
			for (uint32_t j = 0; j < data[i].size(); j++) {
				struct datapt *pt = data[i].at(j);
				fprintf(resp_fp_csv, "%ld %ld\n", pt->qps, pt->lat);
				delete pt;
			}
		}
		
		delete[] data;

		fclose(resp_fp_csv);
	}

	close(mt_kq);
	
	return 0;
}

