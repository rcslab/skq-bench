#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

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

#include "../common.h"

using namespace std;

#define ARG_COUNT 3
#define RECV_BUFFER_SIZE strlen(SERVER_STRING)
//#define PRINT_SERVER_ECHO

vector<thread> threads;
vector<unique_ptr<atomic<long>>> response_counter_total;
int threads_total, test_launch_time = 0;
int conns_total;
int status;
bool quit = false;
atomic<bool> discnt;
atomic<bool> test_begin;
in_addr_t server_ip, mgr_ip, self_ip;
struct timespec kq_tmout = {0, 0};
mutex connect_lock;

void 
client_thread(in_addr_t self_ip_addr, int port, int id, int conn_count)
{
	int conn_fd, time_gap, conn_idx;
	struct sockaddr_in server_addr, my_addr;	
	char *data = new char[RECV_BUFFER_SIZE];
	struct kevent event;
	struct kevent *tevent = (struct kevent *)malloc(sizeof(struct kevent) * SERVER_SENDING_BATCH_NUM * 10);
	bool first_send = true;
	vector<int> conns;
	vector<vector<uint64_t>> response_counter; /* avg/count/last timestamp */

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

			conns.push_back(conn_fd);
			usleep(50);
			break;
		}
		response_counter.push_back({0, 0, 0});	
		//set cnt_flag to true means there is at least one kevent registered to kq
	}

	connect_lock.unlock();
	printf("Thread %d has established %d connections.\n", id+1, conn_count);

	while (!test_begin) {
		sleep(1);
	}

	while (!discnt) {
		for (int i=0;i<conns.size();i+=SERVER_SENDING_BATCH_NUM) {
			if (discnt) {
				break;
			}
			int je = SERVER_SENDING_BATCH_NUM < (conns.size()-i) ? SERVER_SENDING_BATCH_NUM: (conns.size()-i);
			for (int j=0;j<je;j++) {
				if (discnt) {
					break;
				}
				conn_idx = i+j;
				if (conns[conn_idx] == -1) {
					continue;
				}
				//only calculate response time when the timestamp is clear
				if (response_counter[conn_idx][2] == 0) {
					status = writebuf(conns[conn_idx], CLIENT_STRING, strlen(CLIENT_STRING));
					if (status < 0) {
						if (errno == EPIPE) {
							close(conns[conn_idx]);
							conns[conn_idx] = -1;
						}
						printf("id>%d:[%d]%d   status: %d   conn-i:%d    j:%d\n", id, conn_idx, conns[conn_idx], status, i, j);
						perror("First send");
					}
					response_counter[conn_idx][2] = get_time_us();
				}

				if (first_send && conns[conn_idx] != -1) {
					EV_SET(&event, conns[conn_idx], EVFILT_READ, EV_ADD, 0, 0, (void *)conn_idx);
					while (true) {
						status = kevent(kq, &event, 1, NULL, 0, NULL);
						if (status < 0) {
							perror("kevent reg for connection in client_thread");
							continue;
						}
						break;
					}
				}
			}

			if (discnt) {
				break;
			}

			int local_ret = kevent(kq, NULL, 0, tevent, SERVER_SENDING_BATCH_NUM * 10, &kq_tmout); 			
			if (local_ret > 0) {
				for (int j=0;j<local_ret;j++) {
					if (discnt) {
						break;
					}
					
					status = readbuf(tevent[j].ident, data, RECV_BUFFER_SIZE);
					if (status < 0) {
						continue;
					}

					conn_idx = (int64_t)(tevent[j].udata);
					time_gap = get_time_us() - response_counter[conn_idx][2];
					if (time_gap > 0) {
						response_counter[conn_idx][0] += time_gap;
						response_counter[conn_idx][1]++;
						response_counter[conn_idx][2] = 0;
					}

#ifdef PRINT_SERVER_ECHO
					printf("EV(%d/%d) :: Thread_ID:%d   Conn_ID:%d   Resp_Time:%ld(TSNow:%d) -> %s\n", j, local_ret, id, conn_idx, time_gap, response_counter[conn_idx][2], data);
#endif
				}
			}
		}

		first_send = false;
		if (discnt) {
			break;
		}
	}

	int discnt_conn_count = 0;
	for (int i=0;i<conns.size();i++) {
		if (conns[i] == -1) {
			discnt_conn_count++;
			continue;
		}
		close(conns[i]);

		long avg = response_counter[i][1] == 0? 0: floor(0.5f + response_counter[i][0] * 1.0f / response_counter[i][1]);
		int idx;
		if (avg < SAMPLING_RESPONSE_TIME_RANGE_LOW || avg > SAMPLING_RESPONSE_TIME_RANGE_HIGH) {
			idx = (avg < SAMPLING_RESPONSE_TIME_RANGE_LOW) ? 0: SAMPLING_RESPONSE_TIME_COUNT+1;
		} else {
			idx = (avg - SAMPLING_RESPONSE_TIME_RANGE_LOW) / ((SAMPLING_RESPONSE_TIME_RANGE_HIGH - SAMPLING_RESPONSE_TIME_RANGE_LOW) / SAMPLING_RESPONSE_TIME_COUNT) + 1;
		}
		(*response_counter_total[idx]) += 1;
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
	int ch;
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
				struct Perf_response_data pr_data;
				memset(pr_data.data, 0, sizeof(pr_data.data));
				discnt = true;
				sleep(1);
				printf("Received stop signal.\n");
				for (int i=0;i<threads_total;i++) {
					threads[i].join();
					printf("Thread %d / %d stopped.\n", i+1, threads_total);	
				}
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
				for (auto &ptr: response_counter_total) {
					ptr = make_unique<atomic<long>>(0);
				}

				for (int i=0;i<threads_total;i++) {
					int connperthread = conns_total / threads_total;
					if (i == threads_total-1) {
						connperthread += conns_total % threads_total;
					}
					threads.push_back(thread(client_thread, self_ip, conn_port, i, connperthread));
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
