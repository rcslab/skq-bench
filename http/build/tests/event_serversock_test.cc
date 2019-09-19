
#include <string>
#include <iostream>
#include <system_error>
#include <thread>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <unistd.h>
#include <fcntl.h>

#include <celestis/debug.h>
#include <celestis/eventloop.h>
#include <celestis/event.h>
#include <celestis/eventserversock.h>
#include <celestis/eventsock.h>

using namespace std;
using namespace Celestis;

#define TESTPORT 5000

void
ClientThread()
{
    int fd;
    int status;

    fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
	perror("socket");
	throw system_error(errno, system_category());
    }

    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(TESTPORT);

    status = ::connect(fd, (struct sockaddr *)&addr, addrlen);
    if (status < 0) {
	perror("connect");
	throw system_error(errno, system_category());
    }
}

void
AcceptCB(EventServerSock &sock, int fd)
{
    LOG("Accepted a socket %d", fd);

    ::close(fd);

    sock.cancel();

    return;
}

int
main(int argc, const char *argv[])
{
    int fd;
    int status;
    EventLoop el;

    Debug_OpenLog("event_serversock.log");

    cout << "ServerSocket test" << endl;

    fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
	perror("socket");
	return 1;
    }

    int optval = 1;
    socklen_t optlen = sizeof(optval);
    status = ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, optlen);
    if (status < 0) {
	perror("setsockopt");
	return 1;
    }
    status = ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &optval, optlen);
    if (status < 0) {
	perror("setsockopt");
	return 1;
    }

    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(TESTPORT);

    status = ::bind(fd, (struct sockaddr *)&addr, addrlen);
    if (status < 0) {
	perror("bind");
	return 1;
    }

    if (::listen(fd, 10) < 0) {
	perror("listen");
	return 1;
    }

    EventServerSock ss = EventServerSock(el, &AcceptCB, fd);

    thread c(ClientThread);

    el.addEvent(&ss);
    el.enterLoop();
    c.join();

    cout << "Done" << endl;

    return 0;
}

