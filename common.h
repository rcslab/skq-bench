
#define DEFAULT_SERVER_CLIENT_CONN_PORT 9898
#define DEFAULT_SERVER_CTL_PORT 9900
#define DEFAULT_CLIENT_CTL_PORT 9901

#define MSG_TEST_STAGE_BEGIN 0
#define MSG_TEST_STAGE_END 1
#define MSG_TEST_STAGE_DONE 2
#define MSG_TEST_STAGE_RETRY 3

#define MSG_TEST_PREPARE 0
#define MSG_TEST_START 1
#define MSG_TEST_STOP 2
#define MSG_TEST_GETDATA 3
#define MSG_TEST_QUIT 4

#define CTRL_MSG_COUNT 6
#define CTRL_MSG_IDX_LAUNCH_TIME 0
#define CTRL_MSG_IDX_CLIENT_THREAD_NUM 1
#define CTRL_MSG_IDX_CLIENT_CONNS_NUM 2
#define CTRL_MSG_IDX_CLIENT_CONNS_EACH_NUM 3
#define CTRL_MSG_IDX_SERVER_THREAD_NUM 4
#define CTRL_MSG_IDX_SERVER_TEST_TYPE 5

#define SAMPLING_FREQ_IN_SEC 2
#define SAMPLING_COUNT_FOR_AVG 25
#define SAMPLING_THRESHOLD 0

#define TEST_SCRIPT_INTERVAL 3

enum Kqueue_type {
	kq_type_one = 0,
	kq_type_multiple = -1
};

struct Ctrl_msg {
	char cmd;
	int param[CTRL_MSG_COUNT];
};

struct Server_info {
	char server_addr[128];
	int port;
};

struct Perf_data {
	int test_type;
	int threads_total;
	int conn_count;
	int ev_count;
};
