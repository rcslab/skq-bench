
#include <string>
#include <iostream>

#include <sys/types.h>
#include <sys/socket.h>

#include <unistd.h>
#include <fcntl.h>

#include <celestis/debug.h>
#include <celestis/eventloop.h>
#include <celestis/event.h>
#include <celestis/eventsock.h>

using namespace std;
using namespace Celestis;

#define BUFSZ 512

void
ServerCB(EventContext &ctx, EventSock &sock)
{
    int fd;
    char buf[6];

    cout << "Server Recieved Message!" << endl;

    sock.read(buf, 5);
    sock.write("HELLO", 5);
    fd = open("event_socket_sendfile_test.cc", O_RDONLY);
    if (fd < 0) {
	perror("open");
	abort();
    }
    sock.sendfile(fd, 0, 5);
    close(fd);
    sock.cancel();
}

void
ClientCB(EventContext &ctx, EventSock &sock)
{
    char buf[6];

    cout << "Client Recieved Message!" << endl;

    sock.read(buf, 5);
    sock.read(buf, 5);
    sock.cancel();
}

int
main(int argc, const char *argv[])
{
    int status;
    EventLoop el;

    Debug_OpenLog("event_socket_sendfile.log");

    cout << "Socket test" << endl;

    int socks[2];
    status = socketpair(AF_UNIX, SOCK_STREAM, 0, socks);
    if (status < 0) {
	perror("socketpair");
	return 1;
    }
    cout << socks[0] << " " << socks[1] << endl;

    EventSock ss = EventSock(el, &ServerCB, socks[0]);
    EventSock cs = EventSock(el, &ClientCB, socks[1]);

    el.addEvent(&ss);
    el.addEvent(&cs);

    cs.write("HELLO", 5);
    el.enterLoop();

    cout << "Done" << endl;

    return 0;
}

