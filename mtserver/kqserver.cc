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
#include <random>

#include "../common.h"

using namespace std;

//#define PRINT_CLIENT_ECHO
#define RECV_BUFFER_SIZE strlen(CLIENT_STRING)
//#define PRINT_EVENT_COUNT_TO_SCREEN


const int enable = 1;
int accept_kq, kq_flag;
int threads_total = -1, conn_count = 0;
bool quit = false, discnt_flag = false, enable_mtkq = false;
bool enable_delay = false;
atomic<bool> perf_enable;

vector<thread> threads;
vector<unique_ptr<atomic<long>>> perf_counter;
vector<int> kqs, listen_fds, conns, server_socks;
vector<int> core_affinity;

struct timespec kq_tmout = {2, 0}, accept_kq_tmout = {15, 0};
struct sockaddr_in server_addr, ctl_addr;

Kqueue_type test_type = kq_type_multiple;

void 
server_socket_prepare(uint32_t *ips, int num, int port) 
{
	int fd, status;
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

        status = listen(fd, 10000);
		if (status < 0) {
			perror("listen");
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
	struct timespec delay_ts;
	int status, server_delay;
	uint64_t timestamp;
	char *data = new char[RECV_BUFFER_SIZE];

	default_random_engine random_gen;
	uniform_int_distribution<int> distribution(0, 99);

	vector<int> conn;
	vector<int> conn_fd;

	int kevent_counter = 0;

	printf("Thread %d / %d started.\n", id+1, threads_total);

	if (kq_instance == -1) {
		// multiple kq per thread mode
		kq_instance = kqueue();
		if (enable_mtkq) {
			status = ioctl(kq_instance, FKQMULTI, &kq_flag);
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
			status = readbuf(tevent.ident, data, RECV_BUFFER_SIZE);

			if (discnt_flag) {
				break;
			}

			if (status < 0) {
				perror("client");
				close(tevent.ident);
				continue;
			} 
#ifdef PRINT_CLIENT_ECHO
			printf("%s\n", data);
#endif
			int rand_num = distribution(random_gen);	
			server_delay = (rand_num < 95 ? 20: (rand_num==95? 200: 50));
			if (enable_delay) {
				timestamp = get_time_us();
				while (true) {
					if (get_time_us() - timestamp > server_delay) {
						break;
					}
				}
			}
			status = writebuf(tevent.ident, SERVER_STRING, strlen(SERVER_STRING));
			if (status < 0) {
				perror("send");
			}

			//after receiving and sending back, we increase the perf counter
			if (perf_enable) {
				(*perf_counter[id])++;
			}
		} else {
			if (discnt_flag) {
				break;
			}
		}
	}

	close(kqs[id]);
	delete [] data;
}

void
usage() {
	printf("-p: connection port\n-c: control port\n-a: CPU affinity (start:end:step)\n");
}

int 
main(int argc, char *argv[]) 
{
	int kq, status;
	int conn_fd = 0, conn_port = -1, ctl_port = -1;
	int mgr_listen_fd = 0, mgr_ctl_fd = 0;
	int next_fd = 0, next_thread = 0, test_launch_time, total_ev = 0, ip_num = -1, ch;
	struct Ctrl_msg ctrl_msg;
	struct Perf_data perf_data;
	int conn_counter = 0;
	char send_buff[10];
	char *data = new char[RECV_BUFFER_SIZE];
	char *num_str = new char[5];
	vector<int> cpua_ctl;// first core, last core, step 
	cpuset_t cpuset;
	
	while ((ch = getopt(argc, argv, "a:p:c:")) != -1) {
		switch (ch) {
			case 'p':
				conn_port = atoi(optarg);
				break;
			case 'c':
				ctl_port = atoi(optarg);
				break;
			case 'a':
				for (int i=0;i<=strlen(optarg);i++) {
					if (i==strlen(optarg) | optarg[i] == ':') {
						cpua_ctl.push_back(atoi(num_str));					
						memset(num_str, 0 , strlen(num_str));
						continue;
					}
					strcat(num_str, &optarg[i]);
				}
				if (cpua_ctl[1] > get_numcpus()-1) {
					printf("core id is beyond your total cores.\n");
					abort();
				}
				printf("CPU core ");
				for (int i=cpua_ctl[0];i<=cpua_ctl[1];i+=cpua_ctl[2]) {
					core_affinity.push_back(i);
					printf(i==cpua_ctl[1]?"%d ":"%d, ", i);
					if (i<cpua_ctl[1] && i+cpua_ctl[2] > cpua_ctl[1]) {
						printf("align the boundary of cpu core ids, your last step is beyond your last core id.\n");
						abort();
					}
				}
				printf("will be used.\n");
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

	delete [] num_str;

	if (core_affinity.size() == 0) {
		for (int i=0;i<get_numcpus();i++) {
			core_affinity.push_back(i);
		}
	}

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
	
	kq_flag = 0;

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
		status = readbuf(mgr_ctl_fd, &ctrl_msg, sizeof(ctrl_msg));
		if (status < 0) {
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

				status = writebuf(mgr_ctl_fd, &perf_data, sizeof(perf_data));
				if (status < 0) {
					perror("Sending perf data");
				}
				continue;
			case MSG_TEST_PREPARE:
				printf("Test will start.\n");
			
				struct Server_info server_info;
				status = readbuf(mgr_ctl_fd, &server_info, sizeof(server_info));
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
				enable_delay = ctrl_msg.param[CTRL_MSG_IDX_ENABLE_SERVER_DELAY];
				kq_flag = ctrl_msg.param[CTRL_MSG_IDX_SERVER_KQ_FLAG];

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
					printf("Server will run in ONE kqueue mode.\n");
					kq = kqueue();
					if (enable_mtkq) {
						status = ioctl(kq, FKQMULTI, &kq_flag);
						if (status == -1) {
							perror("MTKQ:");
							abort();
						}
					}
				} else {
					if (enable_mtkq) {
						printf("Will run with kqueue flag %d.\n", kq_flag);
					}
				}

				perf_counter.resize(threads_total);
				for (auto &p : perf_counter) {
					p = make_unique<atomic<long>>(0);
				}
				
				CPU_ZERO(&cpuset);
				for (int i=0;i<threads_total;i++) {
					threads.push_back(thread(work_thread, kq, i));

					// will replace this with the affinity policy later
					int core_id = i % core_affinity.size();

					CPU_SET(core_affinity[core_id], &cpuset);
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
				accept_kq = kqueue();
				if (accept_kq < 0) {
					perror("accept kq");
					abort();
				}

				struct kevent event, accept_event, accept_tevent;
				while (true) {
					EV_SET(&accept_event, listen_fds[next_fd], EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, NULL);
					while (true) {
						if (kevent(accept_kq, &accept_event, 1, NULL, 0, NULL) < 0) {
							perror("accept ev");
							usleep(200);
							continue;
						}
						break;
					}	

					int local_ret = kevent(accept_kq, NULL, 0, &accept_tevent, 1, &accept_kq_tmout);
					if (local_ret <= 0) {
						printf("Accepting session closed due to timeout. %d connection(s) will be dropped.\n", conn_counter); 
						break;
					}

					int curr_fd = accept(listen_fds[next_fd], (struct sockaddr*)NULL, NULL);
					if (curr_fd < 0) {
						perror("accept in main thread");
						continue;
					}

					conn_counter -= 1;
					if ((conn_count-conn_counter)%(conn_count / ctrl_msg.param[CTRL_MSG_IDX_CLIENT_NUM]) == 0) {
						next_fd++;
						if (next_fd >= listen_fds.size()) {
							next_fd = 0;
						}
					}

					status = readbuf(curr_fd, data, RECV_BUFFER_SIZE);
					if (status < 0) {
						perror("Connection test");
						close(curr_fd);
						break;
					}

					struct timeval tv;
					tv.tv_sec = 2;
					tv.tv_usec = 0;
					status = setsockopt(curr_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
					if (status < 0) {
						perror("setsocketopt rcvtimeo in main thread");
					}

					EV_SET(&event, curr_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
					status = kevent(kqs[next_thread], &event, 1, NULL, 0, NULL);
					if (status < 0) {
						perror("kevent for connection in main thread");
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
				close(accept_kq);

				ctrl_msg.cmd = MSG_TEST_START;
				status = writebuf(mgr_ctl_fd, &ctrl_msg, sizeof(ctrl_msg));
				if (status < 0) {
					perror("Sending ready signal");
				}
				break;
			default:
				break;
		}	
		
	}

	delete[] data;
	close(mgr_ctl_fd);
	return 0;
}
