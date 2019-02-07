/*
 * 
 * New thread(maybe main) to do rusage to capture cpu usage etc
 * Add sysctl kern.sched.cpu_topology <- to bind thread to specfic core and add toggles for cores/ht cores
 *
 */

#include <sys/event.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "utils.h"
#include <signal.h>
#include <algorithm>
#include <atomic>
#include <time.h>
#include <pthread_np.h>

using namespace std;

//#define PRINT_CLIENT_ECHO
#define CLIENT_RECV_BUFFER_SIZE 10240
#define ARG_COUNT 3

enum Kqueue_type {
	kq_type_one = 0,
	kq_type_multiple = -1
};

int threads_total = -1;
vector<thread> threads;
vector<int> kqs;

vector<unique_ptr<atomic<long>>> perf_counter;
Kqueue_type test_type = kq_type_multiple;

void 
mon_thread() 
{	
	long total_ev;
	//sample from perf_counter every 1 sec and write it to csv
	printf("Monitor thread is live!\n");
	while (true) {
		total_ev = 0;
		sleep(1);	
		system("clear");
		printf("%s kqueue mode.\n", (test_type == kq_type_one ? "Single": "Multiple"));

		for (int i=0;i<threads_total;i++) {
			long val = *perf_counter[i];
			total_ev += val;

			printf("Thread %d :: Events %ld\n", i, val);

			*(perf_counter[i]) = 0;
		}
		printf("kqueue total: %ld\n", total_ev);
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
		kq_instance = kqueue();
	}
	if (kq_instance < 0) {
		perror("kqueue");
		abort();
	}
	kqs[id] = kq_instance;

	while (true) {
		int local_ret = kevent(kq_instance, NULL, 0, &tevent, 1, NULL);
		if (local_ret > 0) {
			int len = recv(tevent.ident, data, CLIENT_RECV_BUFFER_SIZE, 0);
			if (len < 0) {
				perror("client");	
				break;
			}
			else if (len == 0) {
				//client disconnected 
				close(tevent.ident);
				break;
			}
#ifdef PRINT_CLIENT_ECHO
			printf("%s\n", data);
#endif
			send(tevent.ident, "echo\0", 5, 0); 

			//after receiving and sending back, we increase the perf counter
			(*perf_counter[id])++;

			EV_SET(&event, tevent.ident, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, NULL);
			status = kevent(kqs[id], &event, 1, NULL, 0, NULL);
			if (status < 0) {
				perror("kevent for connection in child thread");
			}
		}
	}

	delete[] data;
}

int 
main(int argc, char *argv[]) 
{
	int kq;
	int listen_fd = 0, conn_fd = 0, conn_port = -1;
	int next_thread = 0;
	int status;
	thread monitor_thread;
	struct sockaddr_in server_addr;
	char send_buff[10];
	int ch;
	cpuset_t cpuset;
	
	while ((ch = getopt(argc, argv, "smp:t:")) != -1) {
		switch (ch) {
			case 's':
				test_type = kq_type_one;
				break;
			case 'm':
				test_type = kq_type_multiple;
				break;
			case 'p':
				conn_port = atoi(optarg);
				break;
			case 't':
				threads_total = atoi(optarg);
				if (threads_total == 0) {
					threads_total = get_numcpus();
				}
				break;
			case '?':
			default:
				printf("-s: single kq\n-m: multiple kq\n-p: port to be used\n-t: threads to be used(0 for all)\n");
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
		conn_port = 9898;
	}

	if (threads_total <= 0) {
		threads_total = get_numcpus();
	}

	// don't raise SIGPIPE when sending into broken TCP connections
	::signal(SIGPIPE, SIG_IGN); 

	CPU_ZERO(&cpuset);

	kqs.reserve(threads_total);
	fill(kqs.begin(), kqs.end(), -1);

	perf_counter.resize(threads_total);
	for (auto &p: perf_counter) {
		p = make_unique<atomic<long>>(0);
	}

	if (test_type == kq_type_multiple) {
		kq = -1;
	} else if (test_type == kq_type_one) {
		kq = kqueue();
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(conn_port);

	listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listen_fd < 0) {
		perror("socket");
		abort();
	}

	int enable = 1;
	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
		perror("setsockopt");
		abort();
	}

	status = ::bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if (status < 0) {
		perror("bind");
		abort();
	}

	status = listen(listen_fd, 10);
	if (status < 0) {
		perror("listen");
		abort();
	}

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
	// otherwise wait 5 and retry
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
		sleep(5);
	}

	monitor_thread = thread(mon_thread);

	printf("***********************************************\n");
	printf("Waiting for connection.\n");

	while (true) {
		struct kevent event;
		int curr_fd = accept(listen_fd, (struct sockaddr*)NULL, NULL);
	
		EV_SET(&event, curr_fd, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, NULL);
		status = kevent(kqs[next_thread], &event, 1, NULL, 0, NULL);
		if (status < 0) {
			perror("kevent for connection in main thread");
			continue;
		}	

		if (++next_thread >= threads_total) {
			next_thread = 0;
		}
	}

	for (int i=0;i<threads_total;i++) {
		threads[i].join();
	}
	monitor_thread.join();

	return 0;
}
