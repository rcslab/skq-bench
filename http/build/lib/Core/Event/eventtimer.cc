
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include <string>
#include <functional>

#include <celestis/debug.h>
#include <celestis/eventloop.h>
#include <celestis/event.h>
#include <celestis/eventtimer.h>

namespace Celestis {

EventTimer::EventTimer(EventLoop &el, EventTimerCB cb, uintptr_t evtid)
    : Event(evtid), isOneshot(false), timeout(0), el(el), cb(cb)
{
}

EventTimer::~EventTimer()
{
    cancel();
}

void
EventTimer::cancel()
{
    if (timeout) {
	el.delEvent((Event *)this);
    }
}

void
EventTimer::oneshot(uint64_t ms)
{
    isOneshot = true;
    timeout = ms;

    el.addEvent(this);
}

void
EventTimer::periodic(uint64_t ms)
{
    isOneshot = false;
    timeout = ms;

    el.addEvent(this);
}

void
EventTimer::addEvent()
{
    int status;
    struct kevent change;

    ASSERT(timeout != 0);

    /* initalize kevent structure */
    EV_SET(&change, eventId, EVFILT_TIMER,
	   EV_ADD | EV_ENABLE | (isOneshot ? EV_ONESHOT : EV_DISPATCH),
	   0, timeout, (void *)eventId);

    status = kevent(el.kq, &change, 1, nullptr, 0, 0);
    ASSERT(status >= 0);
}

void
EventTimer::delEvent()
{
    int status;
    struct kevent change;

    ASSERT(timeout != 0);
    timeout = 0;

    if (isOneshot)
	return;

    /* initalize kevent structure */
    EV_SET(&change, eventId, EVFILT_TIMER,
	   EV_DELETE | EV_DISABLE,
	   0, 0, (void *)eventId);

    status = kevent(el.kq, &change, 1, nullptr, 0, 0);
    ASSERT(status >= 0);
}

void
EventTimer::callback(EventContext &ctx, struct kevent &ev)
{
    cb(*this);

    if (timeout != 0 && !isOneshot && !el.multikq) {
	struct kevent &ch = ctx.alloc();

	/* initalize kevent structure */
	EV_SET(&ch, eventId, EVFILT_TIMER,
	       EV_ENABLE | (isOneshot ? EV_ONESHOT : EV_DISPATCH),
	       0, timeout, (void *)eventId);
    }
}

};

