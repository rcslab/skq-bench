
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include <celestis/debug.h>
#include <celestis/eventloop.h>
#include <celestis/event.h>

#define UPDATES_PER_CALL    128
#define EVENTS_PER_CALL	    64

namespace Celestis {

EventLoop::EventLoop()
{
    multikq = false;
    nextId = 1;

    kq = kqueue();
    if (kq == -1) {
	perror("kqueue");
	abort();
    }
}

EventLoop::~EventLoop()
{
    // Cancel all events

    close(kq);
}

void
EventLoop::enterLoop()
{
    int status;
    int nev;
    struct kevent event[EVENTS_PER_CALL];     /* event that was triggered */
    EventContext ctx = EventContext();

    for (;;) {
	if (events.size() == 0)
	    return;

	nev = kevent(kq,
		     (ctx.nev) ? &ctx.event[0] : nullptr, ctx.nev,
		     &event[0], EVENTS_PER_CALL, nullptr);
	if (nev < 0) {
	    perror("kevent()");
	    abort();
	}

	ctx.nev = 0;

	// XXX: lio_listio to read from all sockets
	

	for (int n = 0; n < nev; n++) {
	    if (event[n].flags & EV_ERROR) {   /* report any error */
		WARNING("EV_ERROR: %s\n", strerror(event[n].data));
		exit(EXIT_FAILURE);
	    }


	    // Lookup & Callback
	    Event *e = events[event[n].ident];
	    if (e) {
		if (event[n].flags & EV_ONESHOT) {
		    delEvent(e);
		}
		e->callback(ctx, event[n]);
	    } else {
		LOG("EventLoop: Event triggered on unregistered event");
	    }

	}

	// lio_listio to write to all sockets
	if (ctx.nio) {
	    status = lio_listio(LIO_NOWAIT, ctx.iovp, ctx.nio, NULL);
	    
	    if (status == 0 || 
		(status == -1 && errno == EIO)) {
		for (int i = 0; i < ctx.nio; i++) {
		    ctx.iov_cb[i](status == 0 ? 0 : aio_error(&ctx.iov[i]), &ctx.iov[i]);
		    //aio_return(ctx.iovp[i]);
		}
	    } else {
		perror("lio_listio()");
		abort();
	    }
	    ctx.nio = 0;
	}

	if (ctx.defer.size()) {
	    for (auto dpc : ctx.defer) {
		dpc();
	    }
	    ctx.defer.clear();
	}
    }
}

void
EventLoop::addEvent(Event *evt)
{
    std::lock_guard<std::mutex> monitor(lock);

    events.emplace(evt->getId(), evt);
    evt->addEvent();
}

void
EventLoop::delEvent(Event *evt)
{
    std::lock_guard<std::mutex> monitor(lock);

    evt->delEvent();
    events.erase(evt->getId());
}

};

