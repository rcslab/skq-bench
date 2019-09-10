
#pragma once

#include <sys/ioctl.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <stdio.h>
#include <vector>
#include <thread>
#include <mutex>

#define DEFAULT_SERVER_CLIENT_CONN_PORT (9898)
#define DEFAULT_CLIENT_CTL_PORT (9901)

#define MSG_TEST_OK (0x1234)
#define MSG_TEST_START (0x2345)
#define MSG_TEST_STOP (0x3456)

#ifndef FKQMULTI
#define FKQMULTI (-1)
#define FKQMPRNT (-1)
#endif

/* XXX: */
static const char* IGNORE_STRING = "IGNOREIG";
static const char* MAGIC_STRING = "MAGICMAG";
#define MESSAGE_LENGTH (8)

static inline int 
get_numcpus() 
{
	return std::thread::hardware_concurrency();
}

static inline uint64_t
get_time_us() 
{
	struct timespec tv;

	clock_gettime(CLOCK_MONOTONIC_FAST, &tv);

	return tv.tv_sec * 1000000 + tv.tv_nsec / 1000;
}

static inline int
readbuf(int fd, void *buf, int len)
{
	int status;

	do {
		if ((status = recv(fd, buf, len, 0)) < 0) {
			return -1;
		} else if (status == 0) { // connection disconnected.
			return -1;
		}
		buf = ((uint8_t *)buf) + status;
		len -= status;
	} while (len > 0);

	return 0;
}

static inline int
writebuf(int fd, const void *buf, int len)
{
	int status;

	do {
		if ((status = send(fd, buf, len, 0)) < 0) {
			perror("send");
			return -1;
		} else if (status == 0) {
			return -1;
		}
		buf = ((uint8_t *)buf) + status;
		len -= status;
	} while (len > 0);

	return 0;
}

#define W(fmt, ...) do { \
	fprintf(stderr, "[WARN] " fmt, ##__VA_ARGS__); \
} while(0)

#define E(fmt, ...) do { \
	fprintf(stderr, "[ERROR] " fmt, ##__VA_ARGS__); \
	exit(1); \
} while(0)

#define V(fmt, ...) do { \
	if (options.verbose) { \
	fprintf(stdout, "[INFO] " fmt, ##__VA_ARGS__); \
	} \
} while(0)
