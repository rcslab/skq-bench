#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <err.h>
#include <time.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/event.h>
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
in_addr_t server_ip, mgr_ip;
struct timespec kq_tmout = {1, 0};

void 
client_thread(in_addr_t ip_addr, int port, int id, int conn_count) 
{
	int conn_fd;
	struct sockaddr_in server_addr;	
	char *data = new char[MSG_BUFFER_SIZE];
	string msg = "hello world from client";
	struct kevent event, tevent;
	bool cnt_flag = false;

	int kq = kqueue();
	if (kq < 0) {
		perror("kqueue");
		return;
	}

	printf("Thread %d / %d started.\n", id+1, threads_total);

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = ip_addr;
	// initialize all the connections
	for (int i=0;i<conn_count;i++) {
		conn_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

		struct timeval tv;
		tv.tv_sec = 5;
		tv.tv_usec = 0;
		setsockopt(conn_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

		if (connect(conn_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
			perror("connection");
			abort();
		}
	
		//EV_SET(&event, conn_fd, EVFILT_TIMER, EV_ADD | EV_ONESHOT, NOTE_ABSTIME, (unsigned long)time(NULL)+test_launch_time, NULL);
		EV_SET(&event, conn_fd, EVFILT_TIMER, EV_ADD | EV_ONESHOT, 0, test_launch_time*1000, NULL);
		while (true) {
			status = kevent(kq, &event, 1, NULL, 0, NULL);
			if (status < 0) {
				perror("kevent for connection in client_thread");
				//close(conn_fd);
				continue;
			}
			break;
		}
			
		//set cnt_flag to true means there is at least one kevent registered to kq
		cnt_flag = true;
	}

	while (!discnt) {
		if (!cnt_flag) {
			printf("No any event registered to kq.\n");
			break;
		}
		
		int local_ret = kevent(kq, NULL, 0, &tevent, 1, &kq_tmout);
		if (local_ret > 0) {
			if (tevent.filter == EVFILT_TIMER) {
				status = send(tevent.ident, msg.c_str(), msg.length(), 0);
				if (status <= 0) {
					close(tevent.ident);
					perror("First send to server");
					continue;
				}
			}
			int len = recv(tevent.ident, data, RECV_BUFFER_SIZE, 0);
			if (len < 0) {
				perror("recv");
				goto set_ev;
			}
			if (len == 0) {
				close(tevent.ident);
				break;
			}

#ifdef PRINT_SERVER_ECHO
			printf("%s\n", data);
#endif	
			status = send(tevent.ident, msg.c_str(), msg.length(), 0);
			if (status <= 0) {
				close(tevent.ident);
				continue;
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

	close(kq);
	delete[] data;
}


int 
main(int argc, char * argv[]) 
{
	struct sockaddr_in mgr_ctl_addr;
	int mgr_ctl_fd;
	struct Ctrl_msg ctrl_msg;
	int conn_port, ctl_port;
	string ip_addr;
	int ch;
	ip_addr = "127.0.0.1";
	conn_port = DEFAULT_CONN_PORT;
	ctl_port = DEFAULT_CTL_PORT;
	bool quit = false;

	while ((ch = getopt(argc, argv, "i:p:c:m:")) != -1) {
		switch (ch) {
			case 'i':
				ip_addr = optarg;
				break;
			case 'm':
				mgr_ip = inet_addr(optarg);
				break;
			case 'p':
				conn_port = atoi(optarg);			
				break;
			case 'c':
				ctl_port = atoi(optarg);	
				break;
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

	mgr_ctl_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (mgr_ctl_fd < 0) {
		perror("ctl socket");
		abort();
	}
	
	mgr_ctl_addr.sin_family = AF_INET;
	mgr_ctl_addr.sin_port = htons(ctl_port);
	mgr_ctl_addr.sin_addr.s_addr = mgr_ip;

	if (connect(mgr_ctl_fd, (struct sockaddr*)&mgr_ctl_addr, sizeof(mgr_ctl_addr)) < 0) {
		perror("ctl connect");
		abort();
	}	

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
			case MSG_TEST_QUIT:
				quit = true;
				break;
			case MSG_TEST_START:
				test_launch_time = ctrl_msg.param[CTRL_MSG_IDX_LAUNCH_TIME];
				threads_total = ctrl_msg.param[CTRL_MSG_IDX_CLIENT_THREAD_NUM];
				conns_total = ctrl_msg.param[CTRL_MSG_IDX_CLIENT_CONNS_NUM];

				discnt = false;
				threads.clear();

				for (int i=0;i<threads_total;i++) {
					threads.push_back(move(thread(client_thread, inet_addr(ip_addr.c_str()), conn_port, i, 
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
