#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread_np.h>
#include <netinet/tcp.h>
#include <numeric>

#include <celestis/http.h>
#include <celestis/debug.h>

namespace Celestis {


HTTPServer::HTTPServer(std::string server_addr, uint16_t server_port, int worker_num, 
	bool lb, bool mkq, int mkq_mode, HTTPCb httpcb)
	: cb(httpcb), s_lb(lb), m_kq(mkq), m_kq_mode(mkq_mode), address(server_addr), port(server_port), numWorker(worker_num)
{
}

HTTPServer::~HTTPServer()
{
}

int
HTTPServer::setSocket(std::string address, uint16_t port)
{
	int status, optval = 1;
	struct sockaddr_in addr;

	this->port = port;
	
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("HTTPServer::setSocket socket");
		return errno;	
	}

	status = ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (status < 0) {
		perror("HTTPServer::setSocket setsockopt SO_REUSEADDR");
		return errno;
	}
	
	status = ::setsockopt(fd, SOL_SOCKET, s_lb? SO_REUSEPORT_LB: SO_REUSEPORT, &optval, sizeof(optval));
	if (status < 0) {
		perror("HTTPServer::setSocket setsockopt SO_REUSEPORT(_LB)");
		return errno;
	}
	if (s_lb) {
		LOG("SO_REUSEPORT_LB enabled.");
	}

	status = ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(int));
	if (status < 0) {
		perror("HTTPServer::setSocket setsockopt TCP_NODELAY");
		return errno;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(address.c_str());
	addr.sin_port = htons(port);

	status = ::bind(fd, (struct sockaddr *)&addr, sizeof(addr));
	if (status < 0) {
		perror("HTTPServer::setSocket bind");
		return errno;
	}

	status = ::listen(fd, 10);
	if (status < 0) {
		perror("HTTPServer::setSocket listen");
		return errno;
	}

	return fd;
}

void
HTTPServer::setCallback(HTTPCb cb)
{
	this->cb = cb;
}

int
HTTPServer::getSessionCount() 
{
	return sessions.size();
}

void
HTTPServer::startWorker()
{
	el.enterLoop();
}

void 
HTTPServer::startServer()
{
	int status;
	cpuset_t cpuset;
	std::vector<int> core_affinity(std::thread::hardware_concurrency());

	if (m_kq) 
		el.enableMkq(m_kq_mode);

	status = setSocket(address, port);
	if (status < 0) {
		return;
	}
	
	EventServerSock ess = EventServerSock(el, [&](EventServerSock& pEss, int fd) {
		this->eventCallback(pEss, fd, HTTPEvent::Accept);
	}, fd);
	

	LOG("HTTP Server running on port %d with %d worker threads.", port, numWorker);
	el.addEvent(&ess);

	if (numWorker > 1) {
		std::iota(std::begin(core_affinity), std::end(core_affinity), 0);	
		CPU_ZERO(&cpuset);
		for (int i=0;i<numWorker;i++) {
			workers.emplace_back(std::thread(&HTTPServer::startWorker, this));	

			/*
			 * Currently I just manually bind each thread to two consecutive cores
			 */
			CPU_SET(core_affinity[(i*2) % core_affinity.size()], &cpuset);
			CPU_SET(core_affinity[(i*2+1) % core_affinity.size()], &cpuset);
			status = pthread_setaffinity_np(workers[i].native_handle(), sizeof(cpuset_t), &cpuset);
			if (status < 0) {
				PERROR("pthread_setaffinity_np error");
			}
		}

		for (int i=0;i<numWorker;i++) {
			workers[i].join();
			LOG("Worker thread %d stopped.", i);
		}
	} else if (numWorker == 1) {
		startWorker();
		LOG("Worker thread 0 stopped.");
	}
}

void
HTTPServer::stopServer()
{
}

void
HTTPServer::eventAddToDeferList(HTTPSession &s)
{
    ASSERT(s.ctx != nullptr);
    s.ctx->appendDefer([&](){
	    /* Clean up current session */
	    sessions.erase(s.fd);
    });
}

void
HTTPServer::eventCallback(Event &ev, int fd, HTTPEvent evType)
{
	switch (evType) {
		case HTTPEvent::Accept: {
			LOG("%s", "New connection accepted.");
			
			auto iter = sessions.emplace(std::piecewise_construct,
										 std::forward_as_tuple(fd),
										 std::forward_as_tuple(*this, el, fd, cb));
			if (iter.second) {
				cb(*this, iter.first->second, evType);
			} else {
				LOG("Duplicated file descriptor. Check HTTPServer::eventCallback.");
			}
	
			break;
		}
		default:
			break;
	}
}


};
