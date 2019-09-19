
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include <aio.h>
#include <sys/signal.h>

#include <string>
#include <functional>

#include <celestis/debug.h>
#include <celestis/eventloop.h>
#include <celestis/event.h>
#include <celestis/eventaio.h>

namespace Celestis {

EventAIO::EventAIO(std::function<void(EventAIO*)> cb)
    : el(nullptr), cb(cb)
{
}

EventAIO::~EventAIO()
{
}

void
EventAIO::cancel()
{
    if (el) {
	el->delEvent((Event *)this);
	el = nullptr;

	// Abort AIO
    }
}

void
EventAIO::read(int fd, void *buf, uint64_t off, uint64_t sz)
{
    int status;

    bzero(&iocb, sizeof(iocb));

    iocb.aio_fildes = fd;
    iocb.aio_buf = buf;
    iocb.aio_offset = off;
    iocb.aio_nbytes = sz;

    iocb.aio_sigevent.sigev_notify = SIGEV_KEVENT;
    iocb.aio_sigevent.sigev_notify_kqueue = el->kq;
    iocb.aio_sigevent.sigev_notify_kevent_flags = EV_ONESHOT;
    iocb.aio_sigevent.sigev_value.sival_ptr = (void *)eventId;

    status = aio_read(&iocb);
    if (status != 0) {
	//throw std::system_error();
	perror("EventAIO::read");
    }
}

void
EventAIO::write(int fd, void *buf, uint64_t off, uint64_t sz)
{
    int status;

    bzero(&iocb, sizeof(iocb));

    iocb.aio_fildes = fd;
    iocb.aio_buf = buf;
    iocb.aio_offset = off;
    iocb.aio_nbytes = sz;

    iocb.aio_sigevent.sigev_notify = SIGEV_KEVENT;
    iocb.aio_sigevent.sigev_notify_kqueue = el->kq;
    iocb.aio_sigevent.sigev_notify_kevent_flags = EV_ONESHOT;
    iocb.aio_sigevent.sigev_value.sival_ptr = (void *)eventId;

    status = aio_write(&iocb);
    if (status != 0) {
	//throw std::system_error();
	perror("EventAIO::write");
    }
}

void
EventAIO::addEvent()
{
    //int status;
    //struct kevent change;

    //this->eventId = evtid;

    /* initalise kevent structure */
    /*EV_SET(&change, evtid, EVFILT_TIMER,
	   EV_ADD | EV_ENABLE | EV_ONESHOT,
	   0, 0, this);

    status = kevent(el->kq, &change, 1, nullptr, 0, 0);
    ASSERT(status >= 0);*/
}

void
EventAIO::delEvent()
{
    //int status;
    //struct kevent change;

    /* initalise kevent structure */
    /*EV_SET(&change, evtid, EVFILT_TIMER,
	   EV_DELETE | EV_DISABLE,
	   0, 0, this);

    status = kevent(el->kq, &change, 1, nullptr, 0, 0);
    ASSERT(status >= 0);*/
}

void
EventAIO::callback(EventContext &ctx, struct kevent &ev)
{
    cb(this);
}

};

