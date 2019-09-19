
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <sys/signal.h>

#include <string>
#include <functional>

#include <celestis/debug.h>
#include <celestis/eventloop.h>
#include <celestis/event.h>
#include <celestis/eventsock.h>
#include <celestis/eventserversock.h>

namespace Celestis {

EventServerSock::EventServerSock(EventLoop &el, EventServerSockCB cb, int fd)
    : Event(fd), fd(fd), el(el), cb(cb)
{
}

EventServerSock::~EventServerSock()
{
    cancel();
}

void
EventServerSock::cancel()
{
    el.delEvent((Event *)this);
}

void
EventServerSock::addEvent()
{
    int status;
    struct kevent change;

    /* initalize kevent structure */
    EV_SET(&change, fd, EVFILT_READ,
	   EV_ADD | EV_ENABLE | EV_DISPATCH,
	   0, 0, this);

    status = kevent(el.kq, &change, 1, nullptr, 0, 0);
    ASSERT(status >= 0);

    enabled = true;
}

void
EventServerSock::delEvent()
{
    int status;
    struct kevent change;

	if (!enabled) {
		return;
	}
    enabled = false;

    /* initalize kevent structure */
    EV_SET(&change, fd, EVFILT_READ,
	   EV_DELETE | EV_DISABLE,
	   0, 0, this);

    status = kevent(el.kq, &change, 1, nullptr, 0, 0);
    ASSERT(status >= 0);
}

void
EventServerSock::callback(EventContext &ctx, struct kevent &ev)
{
    for (int i = 0; i < ev.data; i++) {
	int conn_fd = ::accept4(fd, NULL, 0, SOCK_NONBLOCK);
	
	if (conn_fd < 0) {
	    if (errno == EWOULDBLOCK || errno == EAGAIN) {
		return;
	    }
	    LOG("EventServerSock accept failed: %m");
	} else {
	    cb(*this, conn_fd);
	}
    }

    if (enabled && !el.multikq) {
	struct kevent &ch = ctx.alloc();

	/* initalize kevent structure */
	EV_SET(&ch, fd, EVFILT_READ,
	       EV_ENABLE | EV_DISPATCH,
	       0, 0, this);
    }
}

};

