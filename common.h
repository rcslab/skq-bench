
#define DEFAULT_CONN_PORT 9898
#define DEFAULT_CTL_PORT 9900

#define MSG_TEST_STAGE_BEGIN 0
#define MSG_TEST_STAGE_END 1
#define MSG_TEST_STAGE_DONE 2
#define MSG_TEST_STAGE_RETRY 3

#define MSG_TEST_START 0
#define MSG_TEST_STOP 1
#define MSG_TEST_GETDATA 2
#define MSG_TEST_QUIT 3

#define CTRL_MSG_COUNT 5
#define CTRL_MSG_IDX_LAUNCH_TIME 0
#define CTRL_MSG_IDX_CLIENT_THREAD_NUM 1
#define CTRL_MSG_IDX_CLIENT_CONNS_NUM 2
#define CTRL_MSG_IDX_SERVER_THREAD_NUM 3
#define CTRL_MSG_IDX_SERVER_TEST_TYPE 4

#define SAMPLING_FREQ_IN_SEC 5
#define SAMPLING_COUNT_FOR_AVG 5
#define SAMPLING_THRESHOLD 0



enum Kqueue_type {
	kq_type_one = 0,
	kq_type_multiple = -1
};

struct Ctrl_msg {
	char cmd;
	int param[CTRL_MSG_COUNT];
};

struct Perf_data {
	int test_type;
	int threads_total;
	int conn_count;
	int ev_count;
};
