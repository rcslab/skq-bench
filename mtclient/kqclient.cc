#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

#include "utils.h"
#include "../common.h"

using namespace std;

#define ARG_COUNT 3
#define MSG_BUFFER_SIZE 10240
#define RECV_BUFFER_SIZE 5
//#define PRINT_SERVER_ECHO 

vector<thread> threads;
int threads_total, test_launch_time = 0;
int conns_total;
int status;
bool quit = false;
atomic<bool> discnt;
atomic<bool> test_begin;
in_addr_t server_ip, mgr_ip, self_ip;
struct timespec kq_tmout = {1, 0};
atomic<int> connect_flag;

void 
client_thread(in_addr_t self_ip_addr, int port, int id, int conn_count)
{
	int conn_fd;
	struct sockaddr_in server_addr, my_addr;	
	char *data = new char[MSG_BUFFER_SIZE];
	string msg = "hello world from client";
	struct kevent event, tevent;
	bool cnt_flag = false;
	vector<int> conns;

	int kq = kqueue();
	if (kq < 0) {
		perror("kqueue");
		return;
	}

	while (true) {
		if (connect_flag >= id) {
			break;
		}
		usleep(100);
	}

	printf("Thread %d / %d started.\n", id+1, threads_total);

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = server_ip;

	my_addr.sin_family = AF_INET;

	conns.clear();
	// initialize all the connections
	for (int i=0;i<conn_count;i++) {
		struct timeval tv;
		tv.tv_sec = 5;
		tv.tv_usec = 0;
		int enable = 1;

		while (true) {
			conn_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (setsockopt(conn_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0) {
				perror("setsockopt rcvtimeo");
			}	
			if (setsockopt(conn_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
				perror("setsockopt reuseaddr");
			}
			if (setsockopt(conn_fd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable)) < 0) {
				perror("setsockopt reuseport");
			}
			my_addr.sin_addr.s_addr = self_ip;

			status = ::bind(conn_fd, (struct sockaddr*)&my_addr, sizeof(my_addr));
			if (status < 0) {
				perror("bind");
				abort();
			}
			if (connect(conn_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
				//printf("(%d/%d) ", i, conn_count);
				perror("connection");
				//abort();
				sleep(2);
				close(conn_fd);
				continue;
			} else {
				usleep(20);
				break;
			}
		}
		conns.push_back(conn_fd);
			
		//set cnt_flag to true means there is at least one kevent registered to kq
		cnt_flag = true;
	}

	connect_flag++;
	printf("Thread %d has established %d connections.\n", id+1, conn_count);

	while (!test_begin) {
		sleep(1);
	}
	for (int i=0;i<conns.size();i++) {
		status = send(conn_fd, msg.c_str(), msg.length(), 0);
		if (status <= 0) {
			perror("First send");
			//close(conn_fd);
			continue;
		}

		EV_SET(&event, conn_fd, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, NULL);
		while (true) {
			status = kevent(kq, &event, 1, NULL, 0, NULL);
			if (status < 0) {
				perror("kevent for connection in client_thread");
				//close(conn_fd);
				continue;
			}
			break;
		}
	}

	while (!discnt) {
		if (!cnt_flag) {
			printf("No any event registered to kq.\n");
			break;
		}
		
		int local_ret = kevent(kq, NULL, 0, &tevent, 1, &kq_tmout);
		if (local_ret > 0) {
			int len = recv(tevent.ident, data, RECV_BUFFER_SIZE, 0);
			if (len < 0) {
				perror("recv");
				goto set_ev;
			}

#ifdef PRINT_SERVER_ECHO
			printf("%s\n", data);
#endif	
			status = send(tevent.ident, msg.c_str(), msg.length(), 0);
			if (status <= 0) {
				if (errno == EPIPE) {
					close(tevent.ident);
					continue;
				}
				perror("send");
			}

set_ev:
			EV_SET(&event, tevent.ident, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, NULL);
			status = kevent(kq, &event, 1, NULL, 0, NULL);
			if (status < 0) {
				perror("kevent for connection in child thread");
				close(tevent.ident);
			}
		}
	}

	for (int i=0;i<conns.size();i++) {
		close(conns[i]);
	}
	close(kq);
	delete[] data;
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
	status = read(mgr_ctl_fd, &server_info, sizeof(server_info));
	if (status <= 0) {
		perror("Read server info");
		abort();
	}

	server_ip = server_info.server_addr[server_info.choice];
	conn_port = server_info.port;
	self_ip = server_info.client_ip;
	printf("Connected to manager.\n");

	while (!quit) {
		printf("Waiting for command.\n");
		status = read(mgr_ctl_fd, &ctrl_msg, sizeof(ctrl_msg));
		if (status < 0) {
			perror("read socket command");
			abort();
		} else if (status == 0) {
			printf("Manager disconnected unexpectedly.\n");
			break;
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

				connect_flag = 0;
				for (int i=0;i<threads_total;i++) {
					threads.push_back(move(thread(client_thread, self_ip, conn_port, i, 
									(i == threads_total-1 ? (conns_total - (conns_total / threads_total)*(threads_total-1)):(conns_total / threads_total)))));
				}

				printf("Next test will begin in %d seconds.\n", test_launch_time);
				sleep(test_launch_time);
				printf("Testing...\n");	
				break;
			default:
				break;
		}
	}

	close(mgr_ctl_fd);
	return 0;
}
