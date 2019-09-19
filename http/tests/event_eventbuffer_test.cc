
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
#include <celestis/eventbuffer.h>

using namespace std;
using namespace Celestis;

#define BUFSZ 512

void
ServerInputCB(EventContext &ctx, EventBuffer &buf)
{
    char data[7];

    LOG("Server Recieved Message!");

    buf.read(data, 7);
    buf.printf("HELLO\r\n");
    buf.printf("WORLD\r\n");
    buf.printf("DONE\r\n");
    buf.flush();
    buf.cancel();
}

void
ServerOutputCB(EventContext &ctx, EventOutputBuffer &buf)
{
    LOG("Server OutputBuffer empty!");
}

void
ClientInputCB(EventContext &ctx, EventBuffer &buf)
{
    LOG("Client Recieved Message!");
    string s = buf.readln();
    LOG("ClientInputCB: %s", s.c_str());
    if (s == "DONE")
	buf.cancel();
}

void
ClientOutputCB(EventContext &ctx, EventOutputBuffer &buf)
{
    LOG("Client OutputBuffer empty!");
}

int
main(int argc, const char *argv[])
{
    EventLoop el;

    Debug_OpenLog("event_eventbuffer.log");

    LOG("Socket test");

    int status;
    int socks[2];

    status = socketpair(AF_UNIX, SOCK_STREAM, 0, socks);
    if (status < 0) {
	throw std::system_error(errno, std::system_category());
    }

    EventSock first = EventSock(el, nullptr, socks[0]);
    EventSock second = EventSock(el, nullptr, socks[1]);

    EventBuffer sb(first, nullptr, &ServerOutputCB);
    sb.setMode(EventBufferMode::EOL_CRLF);
    sb.setInputCB([&](EventContext &ctx, EventInputBuffer &buf){ ServerInputCB(ctx, sb); });

    EventBuffer cb(second, nullptr, &ClientOutputCB);
    cb.setMode(EventBufferMode::EOL_CRLF);
    cb.setInputCB([&](EventContext &ctx, EventInputBuffer &buf){ ClientInputCB(ctx, cb); });

    el.addEvent(&first);
    el.addEvent(&second);

    second.write("HELLO\r\n", 7);
    el.enterLoop();

    LOG("Done");

    return 0;
}

