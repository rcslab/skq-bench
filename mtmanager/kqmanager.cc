/*
 * change manager to connect to both c/s
 * c/s will accept manager connection.
 *
 */

#include <stdio.h>
#include <string.h>

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


using namespace std;

string server_hostname;
string server_ip;
int server_ctlport, client_ctlport, conn_port;
string script_name = "";
string output_name = "test.csv";
int client_count;
vector<string> client_hostnames, client_ips;
bool script_test_mode = false, csv_output = false;
int server_fd;
int status;
vector<int> client_fds;
int launch_time, test_type, server_threads_total, client_threads_total, conns_total, conns_each;
int server_ip_num = -1;
vector<in_addr_t> server_ips;
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
	ctrl_msg.param[CTRL_MSG_IDX_CLIENT_CONNS_EACH_NUM] = conns_each;
	ctrl_msg.param[CTRL_MSG_IDX_CLIENT_NUM] = client_count;
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
client_prepare()
{
	struct Ctrl_msg ctrl_msg = build_ctrl_msg(MSG_TEST_PREPARE);
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
	int server_choice = 0, curr_client_count = 0, c_fd;
	struct sockaddr_in csock_addr;
	struct Server_info server_info;
	
	for (int i=0;i<server_ip_num;i++) {
		server_info.server_addr[i] = server_ips[i];	
	}
	server_info.port = conn_port;
	server_info.ip_count = server_ip_num;

	for (int i=0;i<client_count;i++) {
		c_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		csock_addr.sin_family = AF_INET;
		csock_addr.sin_addr.s_addr = inet_addr(client_ips[i].c_str());
		csock_addr.sin_port = htons(client_ctlport);

		server_info.client_ip = inet_addr(client_ips[i].c_str());
		server_info.choice = server_choice++;
		if (server_choice >= server_ip_num) {
			server_choice = 0;
		}

		printf("Connecting to client %s(%s)...\n", client_hostnames[i].c_str(), client_ips[i].c_str());
		status = -1;
		while (status < 0) {
			status = connect(c_fd, (struct sockaddr*)&csock_addr, sizeof(csock_addr));
			if (status != 0) {
				perror("Connect to client:");
				sleep(1);
				continue;
			}
			status = write(c_fd, &server_info, sizeof(server_info));
			if (status < 0) {
				perror("Send server info to client:");
				close(c_fd);
				continue;
			}
		}

		client_fds.push_back(c_fd);
		curr_client_count++;
		printf("Client connected %d/%d.\n", curr_client_count, client_count);
	}
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
init_server_ips()
{
	in_addr_t first_ip = inet_addr(server_ip.c_str());
	for (int i=0;i<server_ip_num;i++) {
		in_addr_t tmp = first_ip + htonl(i);
		server_ips.push_back(tmp);
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
server_prepare()
{
	struct Ctrl_msg ctrl_msg = build_ctrl_msg(MSG_TEST_PREPARE);
	status = write(server_fd, &ctrl_msg, sizeof(ctrl_msg));
	if (status < 0) {
		perror("server_start write");
	}

	struct Server_info server_info;
	for (int i=0;i<server_ip_num;i++) {
		server_info.server_addr[i] = server_ips[i];
	}
	server_info.ip_count = server_ip_num;
	server_info.port = conn_port;

	status = write(server_fd, &server_info, sizeof(server_info));
	if (status < 0) {
		perror("server_start send ip");
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

	samp = perf_data.ev_count;
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
	
	printf("Connecting to server %s(%s)...\n", server_hostname.c_str(), server_ip.c_str());
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

void
server_client_start() {
	struct Ctrl_msg ctrl_msg;
	status = read(server_fd, &ctrl_msg, sizeof(ctrl_msg));
	if (status <= 0) {
		perror("Read server status data");
		return;
	}
	for (int i=0;i<client_count;i++) {
		status = write(client_fds[i], &ctrl_msg, sizeof(ctrl_msg));
		if (status <= 0) {
			perror("Send test ready data");
			return;
		}
	}
}

int
main(int argc, char* argv[]) 
{
	int ch;
	FILE *fp, *fp_csv;
	thread s_thr, c_thr;
	server_ctlport = DEFAULT_SERVER_CTL_PORT;
	conn_port = DEFAULT_SERVER_CLIENT_CONN_PORT;
	client_ctlport = DEFAULT_CLIENT_CTL_PORT;
	server_ip_num = 1;
	while ((ch = getopt(argc, argv, "s:r:P:i:c:p:n:f:o:h")) != -1) {
		switch (ch) {
			case 's':
				server_hostname = optarg;
				server_ip = get_ip_from_hostname(server_hostname);
				break;
			case 'r':
				server_ip_num = atoi(optarg);			
				break;
			case 'P':
				server_ctlport = atoi(optarg);
				break;
			case 'i':
				conn_port = atoi(optarg);
				break;
			case 'c':
				client_hostnames.push_back(optarg);
				client_ips.push_back(get_ip_from_hostname(optarg));
				break;
			case 'p':
				client_ctlport = atoi(optarg);
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
			case 'h':
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
	if (client_count != client_hostnames.size()) {
		printf("Please specify %d client addresses. Only received %ld.\n", client_count, client_hostnames.size());
		exit(1);
	}

	::signal(SIGPIPE, SIG_IGN);


	init_server_ips();

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
			if (fscanf(fp, "%d", &ch) == 0) {
				ch = 0;
			}
			if (ch == 0) {
				server_quit();
				client_quit();
				break;
			} else if (ch == 2) {
				int sleep_time = 0;
				fscanf(fp, "%d", &sleep_time);
				sleep(sleep_time);
				continue;
			}
			
			printf("Wait %d secs for the next test case.\n", TEST_SCRIPT_INTERVAL);
			sleep(TEST_SCRIPT_INTERVAL);
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
		if (conns_total % client_count != 0) {
			conns_total = conns_total / client_count * (client_count + 1);
			printf("[Info] Total connection number will be adjusted to %d.\n", conns_total); 
		}

		printf("Test: %s kqueue(s) with %d server threads, %d client threads, and %d connections.\n",\
				(test_type == 0?"Multiple":"Single"), server_threads_total, client_threads_total, conns_total);
		printf("Test will begin in %d sec(s).\n", launch_time);

		test_type--;
		conns_each = conns_total / client_count;

		server_prepare();
		printf("Server started.\n");
		client_prepare();
		printf("Client started.\n");
		server_client_start();
		printf("Test begin.\n");

		while (samp_count < SAMPLING_COUNT_FOR_AVG) {
			sleep(SAMPLING_FREQ_IN_SEC);
			printf("\r[%d/%d]Sampling..", samp_count+1, SAMPLING_COUNT_FOR_AVG);
			server_get_data(true);

			if (csv_output) {
				printf("\r[%d/%d]Writing to file...\n", samp_count, SAMPLING_COUNT_FOR_AVG);
				fprintf(fp_csv, "%c, %d, %d, %d\n", (test_type==0? '1': 'n'), server_threads_total, conns_total, samp);
				fflush(fp_csv);
				printf("[%d/%d]Done.\n", samp_count, SAMPLING_COUNT_FOR_AVG);
			}
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
