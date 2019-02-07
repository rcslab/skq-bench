/*
 *1. Modify this looks like multiple kq server
 *2. for each of thread, create ~1000 connections.
 */
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <vector>
#include <string.h>
#include <thread>
#include <err.h>
#include <unistd.h>
#include "utils.h"

using namespace std;

#define ARG_COUNT 3
#define MSG_BUFFER_SIZE 10240
//#define PRINT_SERVER_ECHO 

vector<thread> threads;
int threads_total;
string ip_addr;
int conn_port;

void client_thread(in_addr_t ip_addr, int port, int id) {
	int conn_fd = 0;
	int err_code;
	struct sockaddr_in server_addr;	
	char *buff = new char[MSG_BUFFER_SIZE];
	string msg = "hello world from client";
	
	conn_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = ip_addr;
	
	if (connect(conn_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
		perror("connection");
		abort();
	}

	while (true) {
		send(conn_fd, msg.c_str(), msg.length(), 0);
		int len = recv(conn_fd, buff, 5, 0);
		if (len < 0) {
			perror("recv");
			continue;
		}
		if (len == 0) {
			goto done;
		}

#ifdef PRINT_SERVER_ECHO
		printf("%s\n", buff);
#endif
	}

done:
	close(conn_fd);
	free(buff);
}


int main(int argc, char * argv[]) {
	int ch;
	threads_total = get_numcpus();
	ip_addr = "127.0.0.1";
	conn_port = 9898;

	while ((ch = getopt(argc, argv, "i:p:t:")) != -1) {
		switch (ch) {
			case 'i':
				ip_addr = optarg;
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
				printf("-i: ip address\n-p: port\n-t: threads to be used(0 for all)\n");\
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

	for (int i=0;i<threads_total;i++) {
		threads.push_back(move(thread(client_thread, inet_addr(ip_addr.c_str()), conn_port, i)));
	}

	for (int i=0;i<threads_total;i++) {
		threads[i].join();
	}

	return 0;
}
