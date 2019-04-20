
#include <sys/ioctl.h>
#include <sys/event.h>

#include <thread>

#define DEFAULT_SERVER_CLIENT_CONN_PORT 9898
#define DEFAULT_SERVER_CTL_PORT 9900
#define DEFAULT_CLIENT_CTL_PORT 9901

#define MSG_TEST_PREPARE 0
#define MSG_TEST_START 1
#define MSG_TEST_STOP 2
#define MSG_TEST_GETDATA 3
#define MSG_TEST_QUIT 4

#define CTRL_MSG_COUNT 11
#define CTRL_MSG_IDX_LAUNCH_TIME 0
#define CTRL_MSG_IDX_CLIENT_NUM 1
#define CTRL_MSG_IDX_CLIENT_THREAD_NUM 2
#define CTRL_MSG_IDX_CLIENT_CONNS_NUM 3
#define CTRL_MSG_IDX_CLIENT_CONNS_EACH_NUM 4
#define CTRL_MSG_IDX_SERVER_THREAD_NUM 5
#define CTRL_MSG_IDX_SERVER_TEST_TYPE 6
#define CTRL_MSG_IDX_ENABLE_MTKQ 7
#define CTRL_MSG_IDX_ENABLE_SERVER_DELAY 8
#define CTRL_MSG_IDX_SERVER_KQ_FLAG 9
#define CTRL_MSG_IDX_CLIENT_CONNS_COOLDOWN_TIME 10

#define SAMPLING_FIRST_SAMPLE_TIME 5

#define SAMPLING_FREQ_IN_SEC 2
#define SAMPLING_COUNT_FOR_AVG 10
#define SAMPLING_THRESHOLD 0

#define DEFAULT_CONNECTION_COOLDOWN_TIME 100
#define DEFAULT_CLIENT_NO_EVENT_SLEEP_TIME 50

#define SAMPLING_RESPONSE_TIME_RANGE_HIGH 400
#define SAMPLING_RESPONSE_TIME_RANGE_LOW 10
#define SAMPLING_RESPONSE_TIME_COUNT 100  //outliner does not included
		
#define TEST_SCRIPT_INTERVAL 3



#ifndef FKQMULTI
#define FKQMULTI  _IOW('f', 89, int)
#endif

static const char* SERVER_STRING = "Hello world from server.";
static const char* CLIENT_STRING = "echo";


enum Kqueue_type {
	kq_type_one = 0,
	kq_type_multiple = -1
};

struct Ctrl_msg {
	char cmd;
	int param[CTRL_MSG_COUNT];
};

struct Server_info {
	uint32_t server_addr[256];
	int ip_count;
	int port;
	int choice;
	uint32_t client_ip;
};

struct Perf_data {
	int test_type;
	int threads_total;
	int conn_count;
	int ev_count;
};

struct Perf_response_data {
	int data[SAMPLING_RESPONSE_TIME_COUNT + 2];
	//int from_id;
};

int 
get_numcpus() 
{
	return std::thread::hardware_concurrency();
}

static int
get_sampling_response_time_range(int resp_time)
{
	if (resp_time < SAMPLING_RESPONSE_TIME_RANGE_LOW) {
		return 0;
	} else if (resp_time > SAMPLING_RESPONSE_TIME_RANGE_HIGH) {
		return SAMPLING_RESPONSE_TIME_COUNT + 1;
	} else {
		return (resp_time - SAMPLING_RESPONSE_TIME_RANGE_LOW) \
			   / ( (SAMPLING_RESPONSE_TIME_RANGE_HIGH-SAMPLING_RESPONSE_TIME_RANGE_LOW)\
			       / SAMPLING_RESPONSE_TIME_COUNT) + 1;
	}
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
			perror("recv");
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







