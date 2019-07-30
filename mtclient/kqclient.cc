#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <assert.h>

#include <unistd.h>
#include <err.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/event.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <atomic>
#include <thread>
#include <iostream>
#include <vector>
#include <queue>

#include "../common.h"

using namespace std;

#define ARG_COUNT 3
#define RECV_BUFFER_SIZE strlen(SERVER_STRING)
//#define PRINT_SERVER_ECHO

vector<std::thread> threads;
vector<int> thread_need_resp_data;
vector<unique_ptr<atomic<long>>> response_counter_total;
int threads_total, test_launch_time = 0, conn_cooldown_time = DEFAULT_CONNECTION_COOLDOWN_TIME;
int conns_total;
bool quit = false;
atomic<bool> discnt;
atomic<bool> test_begin;
in_addr_t server_ip, mgr_ip, self_ip;
mutex connect_lock;

int
network_send(int *fd, const char *str, const char *prrstr) {
	int status;
	status = writebuf(*fd, str, strlen(str));
	if (status < 0) {
		switch (errno) {
			case ECONNREFUSED:
			case EPIPE:
				perror(prrstr);
				close(*fd);
				break;
		}
	}
	return status;
}

void 
client_thread(in_addr_t self_ip_addr, int port, int id, int conn_count)
{
	struct waittime_conn {
		int fd;
		uint64_t exp;
		int idx;
	} conn;
	int conn_fd, time_gap, conn_idx, idx, poll_nums, status;
	long kq_next_ev_time;
	queue<struct waittime_conn> waittime_queue;
	struct timespec kq_tmout = {0, 0};
	struct sockaddr_in server_addr, my_addr;	
	char *data = new char[RECV_BUFFER_SIZE];
	struct kevent event;
	struct kevent *tevent = (struct kevent *)malloc(sizeof(struct kevent) * conn_count);
	bool first_send = true;
	vector<int> conns;
	vector<int> timers;
	vector<int> response_ts; 
	vector<int> response_counter;
	response_counter.clear();
	response_counter.resize(SAMPLING_RESPONSE_TIME_COUNT + 2);

	int kq = kqueue();
	if (kq < 0) {
		perror("kqueue");
		return;
	}

	connect_lock.lock();

	printf("Thread %d / %d started.\n", id+1, threads_total);

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = server_ip;

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(0);

	conns.clear();
	timers.clear();
	// initialize all the connections	

	for (int i=0;i<conn_count;i++) {
		while (true) {
			int enable = 1;
			struct timeval tv = { .tv_sec = 5, tv.tv_usec = 0 };

			conn_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (conn_fd == -1) {
				printf("Error in creating socket, will retry.\n");
				continue;
			}

			if (setsockopt(conn_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0) {
				perror("setsockopt rcvtimeo");
			}	

			if (setsockopt(conn_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
				perror("setsockopt reuseaddr");
			}
			if (setsockopt(conn_fd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) < 0) {
				perror("setsockopt reuseport");
			}
			my_addr.sin_addr.s_addr = self_ip;

			status = ::bind(conn_fd, (struct sockaddr*)&my_addr, sizeof(my_addr));
			if (status < 0) {
				perror("bind");
				abort();
			}
		
			status = connect(conn_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
			if (status != 0) {
				perror("connection");
				sleep(1);
				close(conn_fd);
				continue;
			}

			if (writebuf(conn_fd, CLIENT_STRING, strlen(CLIENT_STRING)) < 0) {
				perror("first send");
				close(conn_fd);
				continue;
			}

			conns.push_back(conn_fd);
			usleep(50);
			break;
		}
		response_ts.push_back(0);	
		//set cnt_flag to true means there is at least one kevent registered to kq
	}

	connect_lock.unlock();
	printf("Thread %d has established %d connections.\n", id+1, conn_count);

	while (!test_begin) {
		sleep(1);
	}

	// do the first send to sever and then use kqueue to handle the remaining pingpong
	for (int i=0;i<conns.size();i++) {
		status = network_send(&conns[i], CLIENT_STRING, "First send");
		if (status < 0) {
			continue;
		}

		response_ts[i] = get_time_us();

		EV_SET(&event, conns[i], EVFILT_READ, EV_ADD, 0, 0, (void*)i);
		status = kevent(kq, &event, 1, NULL, 0, NULL);
		if (status < 0) {
			perror("conn fd event kq reg problem");
		}
	}
	
	kq_next_ev_time = 0;
	while (!discnt) {
		if (kq_next_ev_time <= get_time_us()) {
			kq_tmout = {0, 0};
		} else {
			// timespec is nanosec
			long d = kq_next_ev_time - get_time_us();
			kq_tmout = {0, (d > 0 ? d: 0) * 1000};
		}

		poll_nums = kevent(kq, NULL, 0, tevent, conn_count, &kq_tmout);
		if (poll_nums > 0) {
			if (discnt) break;

			for (int i=0;i<poll_nums;i++) {
				conn_idx = (uint64_t)(tevent[i].udata);	

				status = readbuf(conns[conn_idx], data, RECV_BUFFER_SIZE);
				if (status < 0) {
					printf("One connection closed due to network problem while recving. fd=%d\n", conns[conn_idx]);
					close(conns[conn_idx]);
					continue;
				}

				// update response time counter
				time_gap = get_time_us() - response_ts[conn_idx];
				idx = get_sampling_response_time_range(time_gap);

				response_counter[idx]++;
				response_ts[conn_idx] = 0;

				// enqueue the conn
				conn.fd = conns[conn_idx];
				conn.idx = conn_idx;
				conn.exp = get_time_us() + conn_cooldown_time;
				waittime_queue.push(conn);	
			}
		} else {
			usleep(DEFAULT_CLIENT_NO_EVENT_SLEEP_TIME);
		}

		// dequeue conn if it expires
		while (!waittime_queue.empty()) {
			if (waittime_queue.front().exp <= get_time_us()) {
				conn = waittime_queue.front();
				waittime_queue.pop();

				status = network_send(&conns[conn_idx], CLIENT_STRING, "Client send");
				if (status < 0) {
					printf("One connection closed due to network problem while sending.\n");
					close(conns[conn_idx]);
					continue;
				}

				response_ts[conn_idx] = get_time_us();
			}
			else {
				kq_next_ev_time = waittime_queue.front().exp;
				break;
			}
		}

		// check if mgr asked for resp data
		if (thread_need_resp_data[id]) {
			for (int i=0;i<response_counter.size();i++) {
				(*response_counter_total[i]) += response_counter[i];
				response_counter[i] = 0;
			}
			thread_need_resp_data[id] = 0;
		}

		if (discnt) break;
	}

	int discnt_conn_count = 0;
	for (int i=0;i<conns.size();i++) {
		if (conns[i] == -1) {
			discnt_conn_count++;
			continue;
		}
		close(conns[i]);
	}
	if (discnt_conn_count > 0) {
		printf("Thread %d: %d connection(s) disconnected during the test.\n", id, discnt_conn_count);
	}

	response_counter.clear();
	close(kq);
	free(tevent);
	delete[] data;
}

void
usage() 
{
	printf("-c: control port\n");
}

int 
main(int argc, char * argv[]) 
{
	struct sockaddr_in mgr_ctl_addr;
	int mgr_ctl_fd;
	int listen_fd;
	struct Ctrl_msg ctrl_msg;
	struct Server_info server_info;
	int conn_port, ctl_port;
	int ch, status;
	conn_port = DEFAULT_SERVER_CLIENT_CONN_PORT;
	ctl_port = DEFAULT_CLIENT_CTL_PORT;
	bool quit = false;

	while ((ch = getopt(argc, argv, "c:h")) != -1) {
		switch (ch) {
			case 'c':
				ctl_port = atoi(optarg);	
				break;
			case 'h':
			case '?':
			default:
				usage();
				exit(1);
				break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0) {
		printf("Too many arguments.\n");
		exit(1);
	}

	::signal(SIGPIPE, SIG_IGN);

	int enable = 1;
	mgr_ctl_addr.sin_family = AF_INET;
	mgr_ctl_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	mgr_ctl_addr.sin_port = htons(ctl_port);
	
	listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listen_fd < 0) {
		perror("socket");
		abort();
	}
	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
		perror("setsockopt reuseaddr");
		abort();
	}
	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) < 0) {
		perror("setsockopt reuseport");
		abort();
	}
	status = ::bind(listen_fd, (struct sockaddr*)&mgr_ctl_addr, sizeof(mgr_ctl_addr));
	if (status < 0) {
		perror("bind");
		abort();
	}
	if (listen(listen_fd, 10) < 0) {
		perror("ctl listen");
		abort();
	}
	
	printf("Waiting for manager connection.\n");

	mgr_ctl_fd = accept(listen_fd, (struct sockaddr*)NULL, NULL);

	close(listen_fd);
	status = readbuf(mgr_ctl_fd, &server_info, sizeof(server_info));
	if (status < 0) {
		perror("Read server info");
		abort();
	}

	server_ip = server_info.server_addr[server_info.choice];
	conn_port = server_info.port;
	self_ip = server_info.client_ip;
	printf("Connected to manager.\n");

	while (!quit) {
		printf("Waiting for command.\n");
		status = readbuf(mgr_ctl_fd, &ctrl_msg, sizeof(ctrl_msg));
		if (status < 0) {
			printf("Manager disconnected unexpectedly.\n");
			abort();
		}

		switch (ctrl_msg.cmd) {
			case MSG_TEST_STOP:	
				discnt = true;
				sleep(1);
				printf("Received stop signal.\n");
				for (int i=0;i<threads_total;i++) {
					threads[i].join();
					printf("Thread %d / %d stopped.\n", i+1, threads_total);	
				}
				break;
			case MSG_TEST_GETDATA:
				printf("Manager asked for resp data.\n");
				struct Perf_response_data pr_data;
				memset(pr_data.data, 0, sizeof(pr_data.data));

				for (int i=0;i<threads_total;i++) {
					thread_need_resp_data[i] = 1;
					while (thread_need_resp_data[i]) {
						usleep(10);
					}
				}

				conn_cooldown_time = ctrl_msg.param[CTRL_MSG_IDX_CLIENT_CONNS_COOLDOWN_TIME];
				printf("The connection cooldown time will be changed to %d us.\n", conn_cooldown_time);

				for (int i=0;i<response_counter_total.size();i++) {
					pr_data.data[i] += *(response_counter_total[i]);
				}
					
				status = writebuf(mgr_ctl_fd, &pr_data, sizeof(pr_data));
				if (status < 0) {
					perror("send response counter data");
				}
				break;
			case MSG_TEST_START:
				test_begin = true;
				break;
			case MSG_TEST_QUIT:
				quit = true;
				break;
			case MSG_TEST_PREPARE:
				test_launch_time = ctrl_msg.param[CTRL_MSG_IDX_LAUNCH_TIME];
				threads_total = ctrl_msg.param[CTRL_MSG_IDX_CLIENT_THREAD_NUM];
				conns_total = ctrl_msg.param[CTRL_MSG_IDX_CLIENT_CONNS_EACH_NUM];

				test_begin = false;
				discnt = false;
				threads.clear();
				response_counter_total.clear();
				response_counter_total.resize(SAMPLING_RESPONSE_TIME_COUNT + 2);
				thread_need_resp_data.clear();
				thread_need_resp_data.resize(threads_total);

				for (auto &ptr: response_counter_total) {
					ptr = make_unique<atomic<long>>(0);
				}

				for (int i=0;i<threads_total;i++) {
					thread_need_resp_data[i] = false;
					int connperthread = conns_total / threads_total;
					if (i == threads_total-1) {
						connperthread += conns_total % threads_total;
					}
					threads.push_back(std::thread(client_thread, self_ip, conn_port, i, connperthread));
				}

				printf("Next test will begin in %d seconds.\n", test_launch_time);
				sleep(test_launch_time);
				printf("Testing...\n");	
				break;
			default:
				printf("Unknown message type!\n");
				abort();
				break;
		}
	}

	close(mgr_ctl_fd);
	return 0;
}
