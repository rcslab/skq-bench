#ifndef COMMON_H
#define COMMON_H

#include <sys/ioctl.h>
#include <sys/event.h>
#include <stdint.h>
#include <sys/socket.h>
#include <thread>
#include <stdio.h>

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



static inline int 
get_numcpus()
{
	return std::thread::hardware_concurrency();
}

static inline uint64_t
get_time_us() 
{
  	struct timespec ts;
  	clock_gettime(CLOCK_REALTIME, &ts);
  	//  clock_gettime(CLOCK_REALTIME, &ts);
  	return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

#endif
