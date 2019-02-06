/*
 * Collect events/persec ? core?
 * create # kqueues= # cores
 * New thread(maybe main) to do rusage to capture cpu usage etc
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

using namespace std;

#define THREADS 2
//#define PRINT_CLIENT_ECHO

enum kqueue_type {
  kq_type_one = 0,
  kq_type_multiple = -1
};

int conns_num = 0;
vector<thread> threads;
vector<int> conns;
kqueue_type test_type;

void client_listener(int n, int kq_m) {
  int local_kq;
  int local_fd, local_ret;
  struct kevent event;
  struct kevent tevent;
  int size = 10240;
  char *data = new char[size];

  printf("New Connection. (%d)\n", n);

  if (kq_m == -1) {
    local_kq = kqueue();
    if (local_kq < 0) {
      perror("kqueue");
      abort();
    }
  } else {
    local_kq = kq_m;
  }

  local_fd = conns[n];
  EV_SET(&event, local_fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, NULL);
  local_ret = kevent(local_kq, &event, 1, NULL, 0, NULL);
  if (local_ret == -1) {
    perror("kevent");
    abort();
  }


  for (;;) {
    int status = write(conns[n], "echo", 5);
    if (status < 0) {
      perror("write");
      abort();
    }

    local_ret = kevent(local_kq, NULL, 0, &tevent, 1, NULL);
    if (local_ret > 0) {
#ifdef PRINT_CLIENT_ECHO
      printf("Received packet.\n");
#endif
      read(local_fd, data, size);      
    }

#ifdef PRINT_CLIENT_ECHO
    cout<<data<<endl;
#endif
  }
}

int main(int argc, char *argv[]) {
  int kq;
  int listenfd = 0, connfd = 0;
  int err_code;
  struct sockaddr_in server_addr;
  char sendBuff[10];

  test_type = kq_type_multiple;

  if (test_type == kq_type_multiple) {
    kq = -1;
  } else if (test_type == kq_type_one) {
    kq = kqueue();
  }

  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  memset(sendBuff, 0, sizeof(sendBuff));

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(8999);

  listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  int enable = 1;
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    printf("setsockopt(SO_REUSEADDR) failed\n");
    ::bind(listenfd, (struct sockaddr*)&server_addr, sizeof(server_addr));

  err_code = listen(listenfd, 10);

  for (;;) {
    printf("***********************************************\n");
    printf("Waiting for connection.\n");
    struct sockaddr_in a;
    conns.push_back(accept(listenfd, (struct sockaddr*)NULL, NULL));
    conns_num++;
    threads.push_back(move(thread(client_listener, conns_num-1, kq)));  
  }

  return 0;
}
