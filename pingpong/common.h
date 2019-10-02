
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

#define PPD_CECHO (1)
struct ppd_echo_arg {
	uint32_t enable_delay;
};

#define PPD_CTOUCH (2)
struct ppd_touch_arg {
	uint32_t touch_cnt;
	uint32_t inc;
};

#define PPD_CRESP (-1)
struct ppd_msg {
	int cmd;
	int dat_len;
	char dat[0];
};


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

static inline int read_msg(int fd, struct ppd_msg **out) 
{
	struct ppd_msg* real;
	struct ppd_msg msg;
	int ret = 0;

	ret = readbuf(fd, &msg, sizeof(struct ppd_msg));

	if (ret < 0) {
		return ret;
	}

	real = (struct ppd_msg *)malloc(sizeof(ppd_msg) + msg.dat_len);

	if (msg.dat_len > 0) {
		ret = readbuf(fd, &real->dat[0], msg.dat_len);

		if (ret < 0) {
			free(real);
			return ret;
		}
	}

	memcpy(real, &msg, sizeof(struct ppd_msg));
	*out = real;
	
	return ret;
}

static inline int write_msg(int fd, const struct ppd_msg *msg) 
{
	return writebuf(fd, msg, sizeof(struct ppd_msg) + msg->dat_len);
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
