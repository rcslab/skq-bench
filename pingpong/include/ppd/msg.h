
#ifndef MSG_H
#define MSG_H

#include <common.h>
#include <thread>

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

#endif