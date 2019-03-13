#include <sys/ioctl.h>
#include <sys/event.h>

#define DEFAULT_SERVER_CLIENT_CONN_PORT 9898
#define DEFAULT_SERVER_CTL_PORT 9900
#define DEFAULT_CLIENT_CTL_PORT 9901

#define MSG_TEST_PREPARE 0
#define MSG_TEST_START 1
#define MSG_TEST_STOP 2
#define MSG_TEST_GETDATA 3
#define MSG_TEST_QUIT 4

#define CTRL_MSG_COUNT 8
#define CTRL_MSG_IDX_LAUNCH_TIME 0
#define CTRL_MSG_IDX_CLIENT_NUM 1
#define CTRL_MSG_IDX_CLIENT_THREAD_NUM 2
#define CTRL_MSG_IDX_CLIENT_CONNS_NUM 3
#define CTRL_MSG_IDX_CLIENT_CONNS_EACH_NUM 4
#define CTRL_MSG_IDX_SERVER_THREAD_NUM 5
#define CTRL_MSG_IDX_SERVER_TEST_TYPE 6
#define CTRL_MSG_IDX_ENABLE_MTKQ 7

#define SAMPLING_FREQ_IN_SEC 2
#define SAMPLING_COUNT_FOR_AVG 5
#define SAMPLING_THRESHOLD 0

#define SAMPLING_RESPONSE_TIME_RANGE_HIGH 10000
#define SAMPLING_RESPONSE_TIME_RANGE_LOW 10
#define SAMPLING_RESPONSE_TIME_COUNT 30  //outliner does not included

#define TEST_SCRIPT_INTERVAL 3

#define TIME_SEC_FACTOR 1000000
#define TIME_USEC_FACTOR 0.001

#ifndef FKQMULTI
#define FKQMULTI  _IO('f', 89)
#endif

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
