#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <pthread_np.h>
#include <fcntl.h>

#include <string>
#include <ctime>
#include <vector>
#include <chrono>
#include <numeric>
#include <thread>
#include <fstream>
#include <sstream>
#include <iostream>

#include <celestis/debug.h>
#include <celestis/http.h>
#include "httpd_common.h"

using namespace Celestis;

#define DEFAULT_SERVER_PORT 19999
#define DEFAULT_SERVER_ADDRESS "0.0.0.0"
#define DEFAULT_SERVER_DOCUMENT_ROOT "/usr/local/www/nginx/"

std::string SERVER_REASON_STR_200 = "OK";

void
usage()
{
	printf("Celestis Httpd\n\n");
	printf("Usage:\n");
	printf("-s addr: Server address\n");
	printf("-p %%d  : Server listen ip\n");
	printf("-w %%d  : Server worker thread\n");
	printf("-d dir : Document root\n");
	printf("-l     : Toggle SO_REUSEPORT_LB\n");
	printf("-m %%d : Multiple KQ with scheduling mode (New mKQ FreeBSD build needed)\n");
	printf("-h     : Show this message\n");
}

int
init_env(Server& server)
{
	if ('/' != server.root[server.root.length()-1]) {
		server.root += '/';
	}

	DIR* dir = opendir(server.root.c_str());
	if (dir) {
		closedir(dir);
	} else {
		if (ENOENT == errno) {
			WARNING("Document root does not exist");
		} else {
			WARNING("Failed to open document root");
		}
		return -1;
	}

	return 0; 
}

void
request_handler(HTTPSession& session, const Server& server)
{
	switch (session.getRequest().getMethod()) {
		case HTTPMethod::GET: {
			int fd, f_size;
			struct stat f_info;
			std::string localPath = server.root + session.getRequest().getRequestTargetPath();
			
			//if (session.getRequest().getRequestTargetPath().find("exit") != std::string::npos) {
			//	exit(0);
			//}
			
			if (!is_path_point_to_file(localPath)) {
				localPath += "index.html";
			}
			
			fd = open(localPath.c_str(), O_RDONLY);
			if (fd < 0) {
				LOG("Failed to open file in document root");
				break;
			}

			if (fstat(fd, &f_info) != 0) {
				LOG("Failed to fstat file in document root");
				break;
			}
			f_size = f_info.st_size;

			session.sendResponseFd(fd, 0, f_size, 200, SERVER_REASON_STR_200); 
			close(fd);
		}	
			break;
		case HTTPMethod::POST:
			break;
		case HTTPMethod::PUT:
		case HTTPMethod::HEAD:
		case HTTPMethod::CONNECT:
		case HTTPMethod::DELETE:
		case HTTPMethod::OPTIONS:
		case HTTPMethod::TRACE:
		case HTTPMethod::PATCH:
		default:
			break;
	}

	if (!session.isKeepAlive()) {
		session.close();
	}
}

void
run_server(Server server_instance, int numWorker)
{
	HTTPServer server(server_instance.address, server_instance.port, numWorker, 
		server_instance.s_lb, server_instance.m_kq, server_instance.m_kq_mode, 
		[&](HTTPServer& svr, HTTPSession& session, HTTPEvent evType) {
			switch (evType) {
				case HTTPEvent::Output:	
					request_handler(session, server_instance);			
					//session.sendResponse(content, 200, SERVER_REASON_STR_200);
					break;
				case HTTPEvent::Accept:
					break;
				case HTTPEvent::Error:
					svr.eventAddToDeferList(session);
					break;	
				default:
					break;
			}
		});

	printf("Server will run at %s:%d.\n", server_instance.address.c_str(), server_instance.port);
	server.startServer();
	printf("Server will now shutdown.\n");
}

int
main(int argc, char* argv[])
{
	int ch, status, numWorker = 1;
	cpuset_t cpuset;
	std::vector<int> core_affinity(std::thread::hardware_concurrency());
	Server server_instance(DEFAULT_SERVER_PORT, "Default", 
		DEFAULT_SERVER_ADDRESS, DEFAULT_SERVER_DOCUMENT_ROOT);
	std::vector<std::thread> servers;

	while ((ch = getopt(argc, argv, "s:p:w:d:m:lh?")) != -1) {
		switch (ch) {
			case 's':
				server_instance.address = optarg;
				break;
			case 'p':
				server_instance.port = atoi(optarg);
				break;
			case 'w':
				numWorker = atoi(optarg);	
				break;
			case 'd':
				server_instance.root = optarg;
				break;
			case 'm':
				/*
				 * If mKQ is true then multiple(n=numWorker) httpd objects
				 * will be created, and each httpd obj will only have one worker
				 */
#ifdef FKQMULTI	
				server_instance.m_kq = true;
				server_instance.m_kq_mode = atoi(optarg);
#else
				printf("FreeBSD with SKQ build needed. Will switch back to legacy mode.\n");
				server_instance.m_kq = false;
#endif
				break;
			case 'l':
				server_instance.s_lb = true;
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
	
	Debug_OpenLog("httpd.log");

	if (0 != argc) {
		WARNING("Too many arguments.\n");
		exit(1);
	}

	if (0 != init_env(server_instance)) {
		WARNING("Failed to initialize server");
		exit(1);
	}

	std::string localPath = "/usr/local/www/nginx/index.html";
	std::ifstream file;
	std::stringstream ss;
	std::string content;
	file.open(localPath);
	ss<<file.rdbuf();
	content = ss.str();
	file.close();

	/*
	 * In multiple KQ mode, each http server obj will only create one worker
	 */
	if (server_instance.m_kq) {
		std::iota(std::begin(core_affinity), std::end(core_affinity), 0);
		CPU_ZERO(&cpuset);
		for (int i=0;i<numWorker;i++) {
			servers.emplace_back(std::thread(&run_server, server_instance, 1));

			/*
			 * Currently I just manually bind each thread to two consective cores
			 */
			CPU_SET(core_affinity[(i*2) % core_affinity.size()], &cpuset);
			CPU_SET(core_affinity[(i*2+1) % core_affinity.size()], &cpuset);
			status = pthread_setaffinity_np(servers[i].native_handle(), sizeof(cpuset_t), &cpuset);
			if (status < 0) {
				PERROR("pthread_setaffinity_np error");
			}
		}

		for (int i=0;i<numWorker;i++) {
			servers[i].join();
			LOG("Http server %d stopped.", i);
		}
	} else {
		run_server(server_instance, numWorker);
	}

	return 0;
}
