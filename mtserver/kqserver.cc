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
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <pthread_np.h>

#include <atomic>
#include <thread>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>

#include "../common.h"
#include "utils.h"

using namespace std;

//#define PRINT_CLIENT_ECHO
#define CLIENT_RECV_BUFFER_SIZE 10240
//#define PRINT_EVENT_COUNT_TO_SCREEN


const int enable = 1;
int threads_total = -1, conn_count = 0;
int status;
bool quit = false, discnt_flag = false, enable_mtkq = false;

atomic<bool> perf_enable;

vector<thread> threads;
vector<unique_ptr<atomic<long>>> perf_counter;
vector<int> kqs, listen_fds, conns, server_socks;

struct timespec kq_tmout = {2, 0};
struct sockaddr_in server_addr, ctl_addr;

Kqueue_type test_type = kq_type_multiple;


void 
server_socket_prepare(uint32_t *ips, int num, int port) 
{
	int fd;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);

	server_socks.clear();
	listen_fds.clear();
	for (int i=0;i<num;i++) {
		server_addr.sin_addr.s_addr = ips[i];
		int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (fd < 0) {
			perror("socket");
			abort();
		}
		fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		listen_fds.push_back(fd);

		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
			perror("setsockopt reuseaddr");
			abort();
		}
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) < 0) {
			perror("setsockopt reuseport");
			abort();
		}

		status = ::bind(fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
		if (status < 0) {
			perror("bind");
			abort();
		}
	}
}

/*
 * kq_instance = -1 means one kqueue mode, otherwise pass kqueue value
 */
void 
work_thread(int kq_instance, int id) 
{
	struct kevent event, tevent;
	int size = CLIENT_RECV_BUFFER_SIZE;
	char *data = new char[size];
	int status;

	vector<int> conn;
	vector<int> conn_fd;

	int kevent_counter = 0;

	printf("Thread %d / %d started.\n", id+1, threads_total);

	if (kq_instance == -1) {
		// multiple kq per thread mode
		kq_instance = kqueue();
		if (enable_mtkq) {
			status = ioctl(kq_instance, FKQMULTI);
			if (status == -1) {
				perror("Oscar Tsalapatis:");
				abort();
			}
		}
	} else {
		// single kq mode	
	}
	
	if (kq_instance < 0) {
		perror("kqueue");
		abort();
	}
	kqs[id] = kq_instance;

	
	while (!discnt_flag) {
		int local_ret = kevent(kq_instance, NULL, 0, &tevent, 1, &kq_tmout);
		if (local_ret > 0) {
			int len = recv(tevent.ident, data, CLIENT_RECV_BUFFER_SIZE, 0);

			if (discnt_flag) {
				break;
			}

			if (len < 0) {
				perror("client");	
				continue;
			} 
#ifdef PRINT_CLIENT_ECHO
			printf("%s\n", data);
#endif
			send(tevent.ident, "echo\0", 5, 0); 

			//after receiving and sending back, we increase the perf counter
			if (perf_enable) {
				(*perf_counter[id])++;
			}
		} else {
			if (discnt_flag) {
				break;
			}
		}
		EV_SET(&event, tevent.ident, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, NULL);
		status = kevent(kqs[id], &event, 1, NULL, 0, NULL);
		if (status < 0) {
			perror("kevent for connection in child thread");
		}
	}

	close(kqs[id]);
	delete[] data;
}

int 
main(int argc, char *argv[]) 
{
	int kq;
	int conn_fd = 0, conn_port = -1, ctl_port = -1;
	int mgr_listen_fd = 0, mgr_ctl_fd = 0;
	int next_fd = 0, next_thread = 0, test_launch_time, total_ev = 0, ip_num = -1, ch;
	struct Ctrl_msg ctrl_msg;
	struct Perf_data perf_data;
	int conn_counter = 0;
	char send_buff[10];
	cpuset_t cpuset;
	
	while ((ch = getopt(argc, argv, "p:c:")) != -1) {
		switch (ch) {
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
		printf("Too many arguments.");
		exit(1);
	}
	if (conn_port <= -1) {
		conn_port = DEFAULT_SERVER_CLIENT_CONN_PORT;
	}
	if (ctl_port <= -1) {
		ctl_port = DEFAULT_SERVER_CTL_PORT;
	}

	// don't raise SIGPIPE when sending into broken TCP connections
	::signal(SIGPIPE, SIG_IGN); 

	// setup monitor thread
	quit = false;
	mgr_listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (mgr_listen_fd < 0) {
		perror("ctl socket");
		abort();
	}

	ctl_addr.sin_family = AF_INET;
	ctl_addr.sin_port = htons(ctl_port);
	ctl_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (setsockopt(mgr_listen_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
		perror("ctl setsockopt");
		abort();
	}

	if (::bind(mgr_listen_fd, (struct sockaddr*)&ctl_addr, sizeof(ctl_addr)) < 0) {
		perror("ctl bind");
		abort();
	}

	if (listen(mgr_listen_fd, 10) < 0) {
		perror("ctl listen");	
	}

	printf("Waiting for manager connection.\n");
	mgr_ctl_fd = accept(mgr_listen_fd, (struct sockaddr*)NULL, NULL);
	close(mgr_listen_fd);
	printf("Manager connected.\n");

	while (!quit) {
		printf("Waiting for command.\n");
		status = read(mgr_ctl_fd, &ctrl_msg, sizeof(ctrl_msg));
		if (status <= 0) {
			perror("read socket command");
			abort();
		}

		switch (ctrl_msg.cmd) {
			case MSG_TEST_QUIT:
				quit = true;
				continue;
			case MSG_TEST_STOP:
				printf("Sampling done.\n");
				discnt_flag = true;
				for (int i=0;i<threads_total;i++) {
					threads[i].join();
					close(kqs[i]);
					printf("Thread %d / %d stopped.\n", i+1, threads_total);
				}
				for (int i=0;i<conn_count;i++) {
					close(conns[i]);
				}
				continue;
			case MSG_TEST_GETDATA:
				printf("Manager asked for some data.\n");
				total_ev = 0;
				perf_enable = true;
				sleep(1);
				perf_enable = false;
				for (auto &p: perf_counter) {
					total_ev += *(p);
					*(p) = 0;
				}

				perf_data.test_type = test_type;
				perf_data.threads_total = threads_total;
				perf_data.conn_count = conn_count;
				perf_data.ev_count = total_ev;

				if (write(mgr_ctl_fd, &perf_data, sizeof(perf_data)) <= 0) {
					perror("Sending perf data");
				}
				continue;
			case MSG_TEST_PREPARE:
				printf("Test will start.\n");
			
				struct Server_info server_info;
				status = read(mgr_ctl_fd, &server_info, sizeof(server_info));
				if (status < 0) {
					perror("read socket server_info");
					abort();
				}
				server_socket_prepare(server_info.server_addr, server_info.ip_count, conn_port);

				discnt_flag = false;

				test_type = (Kqueue_type)ctrl_msg.param[CTRL_MSG_IDX_SERVER_TEST_TYPE];
				threads_total = ctrl_msg.param[CTRL_MSG_IDX_SERVER_THREAD_NUM];
				test_launch_time = ctrl_msg.param[CTRL_MSG_IDX_LAUNCH_TIME];
				conn_count = ctrl_msg.param[CTRL_MSG_IDX_CLIENT_CONNS_NUM];
				enable_mtkq = ctrl_msg.param[CTRL_MSG_IDX_ENABLE_MTKQ];

				perf_counter.clear();
				kqs.clear();
				threads.clear();
				perf_enable = false;
				total_ev = 0;
				conns.clear();

				for (int i=0;i<threads_total;i++) {
					kqs.push_back(-1);
				}
				
				kq = -1;
				if (test_type == kq_type_one) {
					kq = kqueue();
					if (enable_mtkq) {
						status = ioctl(kq, FKQMULTI);
						if (status == -1) {
							perror("MTKQ:");
							abort();
						}
					}
				}

				perf_counter.resize(threads_total);
				for (auto &p: perf_counter) {
					p = make_unique<atomic<long>>(0);
				}
	
				CPU_ZERO(&cpuset);
				for (int i=0;i<threads_total;i++) {
					threads.push_back(thread(work_thread, kq, i));

					// will replace this with the affinity policy later
					int core_id = i % get_numcpus();

					CPU_SET(core_id, &cpuset);
					status = pthread_setaffinity_np(threads[i].native_handle(), sizeof(cpuset_t), &cpuset);
					if (status < 0) {
						perror("pthread_setaffinity_np");
					}
				}

				// check if all kqueue threads have been initialized.
				while (true) {
					bool flag = false;
					for (int i=0;i<threads_total;i++) {
						if (kqs[i] == -1) {
							flag = true;
							break;
						}
					}
					if (!flag) {
						break;
					}
					sleep(1);
				}

				conn_counter = conn_count;
				next_thread = 0;
				next_fd = 0;

				status = listen(listen_fds[0], 10000);
				if (status < 0) {
					perror("listen");
					abort();
				}

				while (true) {
					struct kevent event;
					int curr_fd = accept(listen_fds[next_fd], (struct sockaddr*)NULL, NULL);
					if (curr_fd < 0) {
						perror("accept in main thread");
						abort();
					}

					conn_counter -= 1;
					if ((conn_count-conn_counter)%(conn_count / ctrl_msg.param[CTRL_MSG_IDX_CLIENT_NUM]) == 0) {
						next_fd++;
						if (next_fd >= listen_fds.size()) {
							next_fd = 0;
						}

						status = listen(listen_fds[next_fd], 10000);
						if (status < 0) {
							perror("listen");
							abort();
						}
					}	

					struct timeval tv;
					tv.tv_sec = 2;
					tv.tv_usec = 0;
					status = setsockopt(curr_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
					if (status < 0) {
						perror("setsocketopt rcvtimeo in main thread");
					}
					if (status < 0) {
						perror("setsocketopt reuseaddr in main thread");
					}

					EV_SET(&event, curr_fd, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, NULL);
					while (true) {
						status = kevent(kqs[next_thread], &event, 1, NULL, 0, NULL);
						if (status < 0) {
							perror("kevent for connection in main thread");
							printf("kq fd: %d, reg'd fd: %d\n", kqs[next_thread], curr_fd);
							printf("Will retry soon.\n");
							usleep(100);
							continue;
						}
						break;
					}

					conns.push_back(curr_fd);
					if (++next_thread >= threads_total) {
						next_thread = 0;
					}
					if (conn_counter <= 0) {
						break;
					}
				}
				for (int i=0;i<listen_fds.size();i++) {
					close(listen_fds[i]);
				}

				ctrl_msg.cmd = MSG_TEST_START;
				if (write(mgr_ctl_fd, &ctrl_msg, sizeof(ctrl_msg)) <= 0) {
					perror("Sending ready signal");
				}
				break;
			default:
				break;
		}	
		
	}

	close(mgr_ctl_fd);
	return 0;
}
