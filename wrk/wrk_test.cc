#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <unistd.h>
#include <signal.h>

#include <string>
#include <sstream>
#include <vector>

using namespace std;

vector<float> percentiles = {50, 90, 99, 99.999};

void
usage()
{
	printf("Httpd testbench\n\n");
	printf("-T       %%d: httpd threads\n");
	printf("-S     addr: httpd address\n");
	printf("-P       %%d: httpd port\n");
	printf("-D     path: httpd document root\n");
	printf("-F affinity: httpd cpu affinity refer CPUSET set affinity option(CPUSET arg -l)\n");
	printf("-H     path: httpd binary path\n");
	printf("-M         : run httpd manually (by yourself)\n");
	printf("-f affinity: wrk cpu affinity refer CPUSET set affinity option(CPUSET arg -l)\n");
	printf("-r       %%d: httpd throughput rate\n");
	printf("-t       %%d: wrk threads\n");
	printf("-d       %%d: wrk run duration\n");
	printf("-c       %%d: wrk connections (total)\n");
	printf("-p     path: wrk access path (e.g. http://localhost:19999/index.html)\n");
	printf("-s   script: wrk script\n");
	printf("-o   output: csv output file\n");
	printf("-v         : verbose mode\n");
	printf("-h         : show this help\n");
}

void
parse_dump_stats(string stats_str, FILE* fp, bool verbose)
{
	int stage = 0;
	bool target_section = false;
	string line;
	stringstream ss(stats_str);
	
	printf("--------------------------------------------------\n");
	while (getline(ss, line, '\n')){
		if (line == "--BEGIN--") {
			target_section = true;
			continue;
		} else if (line == "--END--") {
			break;
		} else {
			if (!target_section) {
				if (verbose) {
					printf("%s\n", line.c_str());
				}
				continue;
			}
		}

		if (target_section) {
			switch (stage) {
				case 0 ... 1:
					if (stage == 1) {
						fprintf(fp, ", ");
					}
					fprintf(fp, "%s", line.c_str());
					for (int i=0;i<percentiles.size();i++) {
						if (getline(ss, line, '\n')) {
							fprintf(fp, ", %s", line.c_str()); 		
						} else {
							printf("Section latency parsing error!\n");
							break;
						}
					}
					stage++;
					break;
				case 2 ... 3:
					fprintf(fp, ", %s", line.c_str());
					stage++;
					break;
				default:
					break;
			}
			fflush(fp);
		}
	}
	fprintf(fp, "\n");
	
	if (verbose) {
		printf("--------------------------------------------------\n");
	}
}

int
main(int argc, char * argv[])
{
	int ch;
	int httpd_thread = 1, httpd_port = 19999;
	int wrk_thread = 1, wrk_duration = 10, wrk_conn = 1, wrk_rate = 100;
	FILE *output_fp, *fp;
	pid_t httpd;
	bool verbose_mode = false, append_mode = false;
	bool httpd_affinity_enable = false, wrk_affinity_enable = false;
	bool manual_httpd_run = false;
	string httpd_addr, httpd_docroot;
	string wrk_script, wrk_path;
	string stats_str, output_file = "default.csv";
	string httpd_path;
	string httpd_affinity, wrk_affinity;
	string cmd, rt = "";
	char buf[100000];

	while ((ch = getopt(argc, argv, "T:S:P:D:F:Mf:t:d:c:s:p:o:r:H:avh?")) != -1) {
		switch (ch) {
			case 'T':
				httpd_thread = atoi(optarg);	
				break;
			case 'S':
				httpd_addr = optarg;
				break;
			case 'P':
				httpd_port = atoi(optarg);
				break;
			case 'D':
				httpd_docroot = optarg;
				break;
			case 'M':
				manual_httpd_run = true;
				break;
			case 'F':
				httpd_affinity = optarg;
				httpd_affinity_enable = true;
				break;
			case 'f':
				wrk_affinity = optarg;
				wrk_affinity_enable = true;
				break;
			case 'r':
				wrk_rate = atoi(optarg);
				break;
			case 't':
				wrk_thread = atoi(optarg);
				break;
			case 'd':
				wrk_duration = atoi(optarg);
				break;
			case 'c':
				wrk_conn = atoi(optarg);
				break;
			case 'p':
				wrk_path = optarg;
				break;
			case 's':
				wrk_script = optarg;
				break;
			case 'o':
				output_file = optarg;
				break;
			case 'v':
				verbose_mode = true;
				break;
			case 'a':
				append_mode = true;
				break;
			case 'H':
				httpd_path = optarg;
				break;
			case 'h':
			case '?':
			default:
				usage();
				exit(0);
				break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0) {
		printf("Too many arguments.\n");
		exit(1);
	}	

	if (!manual_httpd_run) {
		httpd = fork();
		if (httpd == 0) {
			execlp(httpd_path.c_str(), httpd_path.c_str(), 
					"-s", httpd_addr.c_str(), "-p", to_string(httpd_port).c_str(),
					"-w", to_string(httpd_thread).c_str(), "-d", httpd_docroot.c_str(), 
					NULL);
			perror("Httpd error");
			exit(1);
		}
	}


	output_fp = fopen(output_file.c_str(), append_mode ? "a": "w");	
	if (!output_fp) {
		perror("Failed to open output CSV file");
		exit(1);
	}

	if (!append_mode) {
		fprintf(output_fp, "Httpd-Thread, Wrk-Thread, Wrk-Connection, ");
		fprintf(output_fp, "Latency-Min, Latency-Max, Latency-Mean, Latency-StDev, ");
		fprintf(output_fp, "Latency-50P, Latency-90P, Latency-99P, Latency-99.999P, ");
		fprintf(output_fp, "Requests-Min, Requests-Max, Requests-Mean, Requests-StDev, ");
		fprintf(output_fp, "Requests-50P, Requests-90P, Requests-99P, Requests-99.999P, ");
		fprintf(output_fp, "Total-Duration, Total-Requests, Total-Bytes, Avg-Requests, ");
		fprintf(output_fp, "Error-Connect, Error-Read, Error-Write, Error-Status>399, Error-Timeout\n");
		fflush(output_fp);
	}

	fprintf(output_fp, "%d, %d, %d, ", httpd_thread, wrk_thread, wrk_conn);

	if (wrk_affinity_enable) {
		cmd = "cpuset -c -l " + wrk_affinity + " ";
	} else {
		cmd = "";
	}
	cmd += (" ./wrk2 -R " + to_string(wrk_rate) + " -c " + to_string(wrk_conn));
	cmd += (" -d " + to_string(wrk_duration) + " -t " + to_string(wrk_thread));
	cmd += (" -s " + wrk_script + " " + wrk_path);
	
	fp = popen(cmd.c_str(), "r");
	while (fgets(buf, 100000, fp) != NULL) {
		rt += buf;
	}
	pclose(fp);

	if (!manual_httpd_run) {
		kill(httpd, SIGTERM);
	}

	parse_dump_stats(rt, output_fp, verbose_mode);

	fclose(output_fp);
	return 0;
}

