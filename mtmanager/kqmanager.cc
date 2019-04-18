/*
 * change manager to connect to both c/s
 * c/s will accept manager connection.
 *
 */

#include <stdio.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/event.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <signal.h>
#include <err.h>
#include <netdb.h>

#include <iostream>
#include <vector>
#include <thread>

#include "../common.h"

using namespace std;

string server_hostname;
string server_ip;
string script_name = "";
string output_name = "test.csv";
string resp_output_name = "resp.csv";
int server_ctlport, client_count, client_ctlport, conn_port;
int server_fd;
int status;
int samp = 0, samp_count = 0;
int launch_time, test_type, server_threads_total, client_threads_total, conns_total, conns_each, kq_flag;
int server_ip_num = -1;
long response_data[SAMPLING_RESPONSE_TIME_COUNT + 2];
bool script_test_mode = false, csv_output = false, resp_output = false, enable_mtkq = false;
bool server_enable_delay = false;
bool server_discnt_flag;
vector<string> client_hostnames, client_ips;
vector<int> client_fds;
vector<in_addr_t> server_ips;

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
	ctrl_msg.param[CTRL_MSG_IDX_ENABLE_MTKQ] = enable_mtkq;
	ctrl_msg.param[CTRL_MSG_IDX_ENABLE_SERVER_DELAY] = server_enable_delay;
	ctrl_msg.param[CTRL_MSG_IDX_SERVER_KQ_FLAG] = kq_flag;
	return ctrl_msg;
}

void
client_get_response_data()
{
	memset(response_data, 0, sizeof(response_data));
	struct Perf_response_data pr_data;
	for (int i=0;i<client_fds.size();i++) {
		status = readbuf(client_fds[i], &pr_data, sizeof(pr_data));
		if (status < 0) {
			perror("client_get_response_data read: Inaccurate data");
			abort();
		}
		for (int j=0;j<SAMPLING_RESPONSE_TIME_COUNT+2;j++) {
			response_data[j] += pr_data.data[j];
		}
	}
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
	client_get_response_data();
}

void
client_prepare()
{
	struct Ctrl_msg ctrl_msg = build_ctrl_msg(MSG_TEST_PREPARE);
	for (int i=0;i<client_fds.size();i++) {
		status = writebuf(client_fds[i], &ctrl_msg, sizeof(ctrl_msg));
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
			status = writebuf(c_fd, &server_info, sizeof(server_info));
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
		status = writebuf(client_fds[i], &ctrl_msg, sizeof(ctrl_msg));
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
	status = writebuf(server_fd, &ctrl_msg, sizeof(ctrl_msg));
	if (status < 0) {
		perror("server_stop write");
	}
}

void
server_prepare()
{
	struct Ctrl_msg ctrl_msg = build_ctrl_msg(MSG_TEST_PREPARE);
	status = writebuf(server_fd, &ctrl_msg, sizeof(ctrl_msg));
	if (status < 0) {
		perror("server_start write");
		abort();
	}

	struct Server_info server_info;
	for (int i=0;i<server_ip_num;i++) {
		server_info.server_addr[i] = server_ips[i];
	}
	server_info.ip_count = server_ip_num;
	server_info.port = conn_port;

	status = writebuf(server_fd, &server_info, sizeof(server_info));
	if (status < 0) {
		perror("server_start send ip");
		abort();
	}
}

void 
server_get_data(bool scn_prt)
{
	struct Ctrl_msg ctrl_msg;
	ctrl_msg.cmd = MSG_TEST_GETDATA;
	status = writebuf(server_fd, &ctrl_msg, sizeof(ctrl_msg));
	if (status < 0) {
		perror("server_get_data write");
		return;
	}

	struct Perf_data perf_data;
	status = readbuf(server_fd, &perf_data, sizeof(perf_data));
	if (status < 0) {
		perror("Read perf data");
		return;
	}

	samp = perf_data.ev_count;
	samp_count++;

	if (scn_prt) {
		system("clear");
		printf("\nLast ev Sample\nkq count: %d\nthreads: %d\nconnections: %d\nevents: %d\nioctl status:%s\nsched flag: %d\n", \
				(perf_data.test_type == kq_type_one? 1: perf_data.threads_total), \
				perf_data.threads_total, perf_data.conn_count, perf_data.ev_count, (enable_mtkq==true? "Enabled":"Disabled"), kq_flag);
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
	status = writebuf(server_fd, &ctrl_msg, sizeof(ctrl_msg));
	if (status < 0) {
		perror("server_quit write");
	}
}

void
server_client_start() {
	struct Ctrl_msg ctrl_msg;
	status = readbuf(server_fd, &ctrl_msg, sizeof(ctrl_msg));
	if (status < 0) {
		perror("Read server status data");
		return;
	}
	for (int i=0;i<client_count;i++) {
		status = writebuf(client_fds[i], &ctrl_msg, sizeof(ctrl_msg));
		if (status < 0) {
			perror("Send test ready data");
			return;
		}
	}
}

void
usage() 
{
	printf("Usage:\n");
	printf("-s: server addr.\n");
	printf("-r: number of server ip.\n"); 
	printf("-P: server ctl port.\n");
	printf("-i: c/s conn port.\n");
	printf("-c: client addr.\n");
	printf("-p: client ctl port.\n");
	printf("-n: number of clients.\n");
	printf("-f: test script name.\n");
	printf("-o: test output file name.\n");
	printf("-e: response time output file name.\n");
	printf("-m: toggle new multi kq mode.\n");
	printf("\n\n-h: show this message.\n");
		
}

std::string 
get_ip_from_hostname(std::string hostname) 
{
	static char rt[100];
	struct in_addr **addr;
	struct hostent *he;

	if ((he = gethostbyname(hostname.c_str())) == NULL) {
		printf("Hostname %s cannot be resolved.\n", hostname.c_str());
		abort();
	}
	addr = (struct in_addr**)he->h_addr_list;
	for (int i=0;addr[i]!=NULL;i++) {
		strcpy(rt, inet_ntoa(*addr[i]));
		return rt;
	}
	return rt;
}

int
main(int argc, char* argv[]) 
{
	int ch;
	FILE *fp, *fp_csv, *resp_fp_csv;
	thread s_thr, c_thr;
	server_ctlport = DEFAULT_SERVER_CTL_PORT;
	conn_port = DEFAULT_SERVER_CLIENT_CONN_PORT;
	client_ctlport = DEFAULT_CLIENT_CTL_PORT;
	server_ip_num = 1;
	kq_flag = 0;
	while ((ch = getopt(argc, argv, "s:r:P:i:c:p:n:f:o:he:w:m:")) != -1) {
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
			case 'e':
				resp_output_name = optarg;
				resp_output = true;
				break;
			case 'm':
				enable_mtkq = true;
				kq_flag = atoi(optarg);
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
		fflush(fp_csv);
	}

	if (resp_output) {
		resp_fp_csv = fopen(resp_output_name.c_str(), "w");
		for (int i=0;i<SAMPLING_RESPONSE_TIME_COUNT + 2;i++) {
			if (i == 0 || i == SAMPLING_RESPONSE_TIME_COUNT + 1) {
				int num = (i==0? SAMPLING_RESPONSE_TIME_RANGE_LOW: SAMPLING_RESPONSE_TIME_RANGE_HIGH);
				fprintf(resp_fp_csv, (i==0? "0-%d, ": "%d-INF\n"), num);	
			} else {
				int num_low = SAMPLING_RESPONSE_TIME_RANGE_LOW + \
							  (i-1)*(SAMPLING_RESPONSE_TIME_RANGE_HIGH-SAMPLING_RESPONSE_TIME_RANGE_LOW)/SAMPLING_RESPONSE_TIME_COUNT;
				int num_high = SAMPLING_RESPONSE_TIME_RANGE_LOW + \
							  (i)*(SAMPLING_RESPONSE_TIME_RANGE_HIGH-SAMPLING_RESPONSE_TIME_RANGE_LOW)/SAMPLING_RESPONSE_TIME_COUNT;
				fprintf(resp_fp_csv, "%d-%d, ", num_low, num_high);	
			}
		}
		fflush(resp_fp_csv);
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
			} else if (ch == 3) {
				fscanf(fp, "%d", &ch);
				printf("Server delay toggled to ");
				if (ch == 1) {
					printf("ON\n");
					server_enable_delay = true;
				} else {
					printf("OFF\n");
					server_enable_delay = false;
				}
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
			} else if (ch == 2) {
				int sleep_time = 0;
				scanf("%d", &sleep_time);
				sleep(sleep_time);
				continue;
			} else if (ch == 3) {
				scanf("%d", &ch);
				printf("Server delay toggled to ");
				if (ch == 1) {
					printf("ON\n");
					server_enable_delay = true;
				} else {
					printf("OFF\n");
					server_enable_delay = false;
				}
				continue;
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

		if (resp_output) {
			for (int i=0;i<SAMPLING_RESPONSE_TIME_COUNT+2;i++) {
				fprintf(resp_fp_csv, (i==SAMPLING_RESPONSE_TIME_COUNT+1? "%ld\n": "%ld, "), response_data[i]);
			}
		}

		printf("\nTest done.\n");
	}

	if (script_test_mode) {
		fclose(fp);
	}
	if (csv_output) {
		fclose(fp_csv);
	}
	if (resp_output) {
		fclose(resp_fp_csv);
	}
}

