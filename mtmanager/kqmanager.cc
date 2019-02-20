#include <stdio.h>

#include <unistd.h>
#include <err.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/event.h>

#include <iostream>
#include <vector>
#include <thread>

#include "utils.h"
#include "../common.h"

#define TEST_SCRIPT_INTERVAL 5

using namespace std;

string server_ip;
int server_ctlport, conn_port;
string script_name = "";
string output_name = "test.csv";
int client_count;
bool script_test_mode = false, csv_output = false;
int server_fd;
int status;
vector<int> client_fds;
int launch_time, test_type, server_threads_total, client_threads_total, conns_total;
bool server_discnt_flag;
int samp = 0, samp_count = 0;

struct Ctrl_msg 
build_ctrl_msg(int cmd)
{
	struct Ctrl_msg ctrl_msg;
	ctrl_msg.cmd = cmd;
	ctrl_msg.param[CTRL_MSG_IDX_LAUNCH_TIME] = launch_time;
	ctrl_msg.param[CTRL_MSG_IDX_SERVER_THREAD_NUM] = server_threads_total;
	ctrl_msg.param[CTRL_MSG_IDX_SERVER_TEST_TYPE] = test_type;
	ctrl_msg.param[CTRL_MSG_IDX_CLIENT_THREAD_NUM] = client_threads_total;
	ctrl_msg.param[CTRL_MSG_IDX_CLIENT_CONNS_NUM] = conns_total;
	return ctrl_msg;
}

void 
client_stop()
{
	struct Ctrl_msg ctrl_msg;
	ctrl_msg.cmd = MSG_TEST_STOP;
	for (int i=0;i<client_fds.size();i++) {
		status = write(client_fds[i], &ctrl_msg, sizeof(ctrl_msg));
		if (status < 0) {
			perror("client_stop write");
			continue;
		}
	}
}

void
client_start()
{
	struct Ctrl_msg ctrl_msg = build_ctrl_msg(MSG_TEST_START);
	for (int i=0;i<client_fds.size();i++) {
		status = write(client_fds[i], &ctrl_msg, sizeof(ctrl_msg));
		if (status < 0) {
			perror("client_start write");
			continue;
		}
	}
}

void
client_make_conn()
{
	int curr_client_count = 0;
	struct sockaddr_in csock_addr;

	csock_addr.sin_family = AF_INET;
	csock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	csock_addr.sin_port = htons(conn_port);

	int listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listen_fd < 0) {
		perror("client listen socket");
		abort();
	}

	int enable = 1;
	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
		perror("client setsockopt");
		abort();
	}
	if (::bind(listen_fd, (struct sockaddr*)&csock_addr, sizeof(csock_addr)) < 0) {
		perror("bind");
		abort();
	}
	if (listen(listen_fd, 10) < 0) {
		perror("client listen");
		abort();
	}

	while (curr_client_count < client_count) {
		int c_fd = accept(listen_fd, (struct sockaddr*)NULL, NULL);
		client_fds.push_back(c_fd);
		curr_client_count++;
		printf("Client connected %d/%d.\n", curr_client_count, client_count);
	}
	close(listen_fd);
}

void 
client_quit() 
{
	struct Ctrl_msg ctrl_msg;
	ctrl_msg.cmd = MSG_TEST_QUIT;
	for (int i=0;i<client_fds.size();i++) {
		status = write(client_fds[i], &ctrl_msg, sizeof(ctrl_msg));
		if (status < 0) {
			perror("client_quit write");
			continue;
		}
		close(client_fds[i]);
	}
}

void 
server_stop()
{
	struct Ctrl_msg ctrl_msg;
	ctrl_msg.cmd = MSG_TEST_STOP;
	status = write(server_fd, &ctrl_msg, sizeof(ctrl_msg));
	if (status < 0) {
		perror("server_stop write");
	}
}

void
server_start()
{
	struct Ctrl_msg ctrl_msg = build_ctrl_msg(MSG_TEST_START);
	status = write(server_fd, &ctrl_msg, sizeof(ctrl_msg));
	if (status < 0) {
		perror("server_start write");
	}
}

void 
server_get_data(bool scn_prt)
{
	struct Ctrl_msg ctrl_msg;
	ctrl_msg.cmd = MSG_TEST_GETDATA;
	status = write(server_fd, &ctrl_msg, sizeof(ctrl_msg));
	if (status < 0) {
		perror("server_get_data write");
		return;
	}
	
	struct Perf_data perf_data;
	status = read(server_fd, &perf_data, sizeof(perf_data));
	if (status <= 0) {
		perror("Read perf data");
		return;
	}

	samp += perf_data.ev_count;
	samp_count++;

	if (scn_prt) {
		system("clear");
		printf("\nLast ev Sample\nkq count: %d\nthreads: %d\nconnections: %d\nevents: %d\n", \
				(perf_data.test_type == kq_type_one? 1: perf_data.threads_total), \
				perf_data.threads_total, perf_data.conn_count, perf_data.ev_count);
	}
}

void
server_make_conn() 
{
	struct sockaddr_in server_addr;
	server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(server_ctlport);
	server_addr.sin_addr.s_addr = inet_addr(server_ip.c_str());

	if (connect(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
		perror("server connection.");
		abort();
	}
	printf("Server connected.\n");
}

void
server_quit()
{
	struct Ctrl_msg ctrl_msg;
	ctrl_msg.cmd = MSG_TEST_QUIT;
	status = write(server_fd, &ctrl_msg, sizeof(ctrl_msg));
	if (status < 0) {
		perror("server_quit write");
	}
}

int
main(int argc, char* argv[]) 
{
	int ch;
	FILE *fp, *fp_csv;
	thread s_thr, c_thr;
	while ((ch = getopt(argc, argv, "s:p:c:n:f:o:")) != -1) {
		switch (ch) {
			case 's':
				server_ip = optarg;
				break;
			case 'p':
				server_ctlport = atoi(optarg);
				break;
			case 'c':
				conn_port = atoi(optarg);
				break;
			case 'n':
				client_count = atoi(optarg);
				break;
			case 'f':
				script_test_mode = true;
				script_name = optarg;
				break;
			case 'o':
				output_name = optarg;
				csv_output = true;
				break;
			case '?':
			default:
				usage();
				exit(1);
				break;
		}
	}
	argc-= optind;
	argv+= optind;

	if (argc != 0) {
		printf("Too many arguments.\n");
		exit(1);
	}

	::signal(SIGPIPE, SIG_IGN);

	client_fds.reserve(client_count);

	server_make_conn();
	client_make_conn();

	if (script_test_mode) {
		printf("Will read test cases from file %s.\n", script_name.c_str());
		fp = fopen(script_name.c_str(), "r");
	}

	if (csv_output) {
		fp_csv = fopen(output_name.c_str(), "w");
		fprintf(fp_csv, "KQueue Count, Threads Count(Server), Connections Count, Events Count\n");
	}


	while (true) {
		samp = 0;
		samp_count = 0;

		if (script_test_mode) {
			//read configuration from file
			fscanf(fp, "%d", &ch);
			if (ch == 0) {
				server_quit();
				client_quit();
				break;
			}
			
			sleep(TEST_SCRIPT_INTERVAL);
			printf("Wait %d secs for the next test case.\n");
			fscanf(fp, "%d %d %d %d %d", &launch_time, &test_type, &server_threads_total, &client_threads_total, &conns_total);
		} else {
			printf("====================================\n");
			printf("start_test(1=yes 0=end) launch_time, test_type(0=mul 1=single), server_threads_total, client_threads_total, conn_number\n");
			printf("====================================\n");
			printf("Test>");

			scanf("%d", &ch);
			if (ch == 0) {
				server_quit();
				client_quit();
				break;
			}
			scanf("%d %d %d %d %d", &launch_time, &test_type, &server_threads_total, &client_threads_total, &conns_total);
		}
		printf("Test: %s kqueue(s) with %d server threads, %d client threads, and %d connections.\n",\
				(test_type == 0?"Multiple":"Single"), server_threads_total, client_threads_total, conns_total);
		printf("Test will begin in %d sec(s).\n", launch_time);

		test_type--;

		server_start();
		printf("Server started.\n");
		client_start();
		printf("Client started.\n");

		while (samp_count < SAMPLING_COUNT_FOR_AVG) {
			sleep(SAMPLING_FREQ_IN_SEC);
			printf("\rSampling %d/%d..", samp_count+1, SAMPLING_COUNT_FOR_AVG);
			server_get_data(true);
		}

		if (csv_output) {
			printf("\rWriting to file...\n");
			fprintf(fp_csv, "%d, %d, %d, %d\n", (test_type==0? 1: server_threads_total), server_threads_total, conns_total, samp / samp_count);
			fflush(fp_csv);
		}

		server_stop();
		client_stop();

		printf("\nTest done.\n");
	}

	if (script_test_mode) {
		fclose(fp);
	}
	if (csv_output) {
		fclose(fp_csv);
	}
}
