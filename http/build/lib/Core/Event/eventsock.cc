
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <aio.h>
#include <unistd.h>

#include <sys/event.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/signal.h>

#include <string>
#include <utility>
#include <functional>
#include <system_error>

#include <celestis/debug.h>
#include <celestis/eventloop.h>
#include <celestis/event.h>
#include <celestis/eventsock.h>
#include <celestis/eventbuffer.h>

namespace Celestis {

EventSock::EventSock(EventLoop &el, EventSockCB cb, int fd)
    : Event(fd), rdShutdown(false), wrShutdown(false),
      rdAvail(0), wrAvail(0), sockStatus(0), fd(fd), el(el), cb(cb), ecb(cb)
{
}

EventSock::~EventSock()
{
    cancel();
}

void
EventSock::cancel()
{
    el.delEvent((Event *)this);
}

int
EventSock::read(void *buf, uint64_t sz)
{
#if 1
    int status;

    status = ::read(fd, buf, sz);

    return status;
#else
    int status;
    struct aiocb io;
    struct aiocb *iov[1];

    memset(&io, 0, sizeof(io));
    io.aio_fildes = fd;
    io.aio_buf = buf;
    io.aio_nbytes = sz;
    io.aio_lio_opcode = LIO_READ;
    iov[0] = &io;

    status = lio_listio(LIO_WAIT, (struct aiocb * const *)&iov, 1, NULL);
    if (status < 0) {
	//throw std::system_error();
	perror("EventSock::read");
	printf("aio_error %d", aio_error(&io));
	perror("AIO: ");
	return status;
    }

    return sz;
#endif
}

int
EventSock::write(const void *buf, uint64_t sz)
{
#if 1
    int status;

    status = ::write(fd, buf, sz);
    if (status < 0) {
	//throw std::system_error();
	perror("EventSock::write");
    }

    return status;
#else
    int status;
    struct aiocb io;
    struct aiocb *iov[1];

    memset(&io, 0, sizeof(io));
    io.aio_fildes = fd;
    io.aio_buf = (void *)buf;
    io.aio_nbytes = sz;
    io.aio_lio_opcode = LIO_WRITE;
    iov[0] = &io;

    status = lio_listio(LIO_WAIT, (struct aiocb * const *)&iov, 1, NULL);
    if (status < 0) {
	//throw std::system_error();
	perror("EventSock::read");
	printf("aio_error %d", aio_error(&io));
	perror("AIO: ");
	return status;
    }
    return sz;
#endif
}

int
EventSock::sendfile(int fd, off_t off, size_t sz)
{
    int status;

    status = ::sendfile(fd, this->fd, off, sz, NULL, NULL, 0);
    if (status < 0) {
	perror("EventSock::sendfile");
    }

    return status;
}

int
EventSock::sendseg(EventBufferSegment &seg)
{
    int status;

    if (seg.fd == -1) {
	status = write(&seg.data[seg.offset], seg.length);
	if (status < 0) {
	    if (errno == EWOULDBLOCK) {
		WARNING("WOULDBLOCK");
		return -1;
	    }
	    PERROR("EventOutputBuffer::flush write failed");
	    return -1;
	}

	seg.offset += status;
	seg.length -= status;

	return status;
    }

    if (seg.length != 0) {
	struct sf_hdtr hdr;
	struct iovec hdr_io;

	hdr.headers = &hdr_io;
	hdr.hdr_cnt = 1;
	hdr.trailers = nullptr;
	hdr.trl_cnt = 0;

	hdr_io.iov_base = &seg.data[seg.offset];
	hdr_io.iov_len = seg.length;

	seg.offset += seg.length;

	status = ::sendfile(seg.fd, fd, seg.off, seg.sz, &hdr, NULL, 0);
	if (status < 0) {
	    PERROR("EventOutputBuffer::flush sendfile failed");
	    return -1;
	}

	return seg.length;
    } else {
	status = ::sendfile(seg.fd, fd, seg.off, seg.sz, NULL, NULL, 0);
	if (status < 0) {
	    PERROR("EventOutputBuffer::flush sendfile failed");
	    return -1;
	}
	return 0;
    }
}

void
EventSock::addEvent()
{
    int status;
    struct kevent change;

    /* initalize kevent structure */
    EV_SET(&change, fd, EVFILT_READ | EVFILT_WRITE,
	   EV_ADD | EV_ENABLE | EV_DISPATCH,
	   0, 0, this);

    status = kevent(el.kq, &change, 1, nullptr, 0, 0);
    ASSERT(status >= 0);

    enabled = true;
}

void
EventSock::delEvent()
{
    int status;
    struct kevent change;

    if (!enabled) {
	return;
    }
    enabled = false;

    /* initalize kevent structure */
    EV_SET(&change, fd, EVFILT_READ | EVFILT_WRITE,
	   EV_DELETE | EV_DISABLE,
	   0, 0, this);

    status = kevent(el.kq, &change, 1, nullptr, 0, 0);
    ASSERT(status >= 0);
}

void
EventSock::callback(EventContext &ctx, struct kevent &ev)
{
    // XXX: Registration errors set this, socket errors set EV_EOF
    if (ev.flags & EV_ERROR) {
	enabled = false;
	rdShutdown = true;
	wrShutdown = true;
	sockStatus = ev.data;
	ecb(ctx, *this);
	return;
    }
    if ((ev.flags & EV_EOF) == EV_EOF ){ //&& ev.fflags != 0) {
	enabled = false;
	rdShutdown = true;
	wrShutdown = true;
	sockStatus = ev.fflags;
	ecb(ctx, *this);
	return;
    }

    if (ev.filter & EVFILT_READ) {
	rdAvail = ev.data;
	if (ev.flags & EV_EOF) {
	    rdShutdown = true;
	}
    }
    if (ev.filter & EVFILT_WRITE) {
	wrAvail = ev.data;
	if (ev.flags & EV_EOF) {
	    wrShutdown = true;
	}
    }
    cb(ctx, *this);
    if (enabled && !el.multikq) {
	struct kevent &ch = ctx.alloc();

	/* initalize kevent structure */
	EV_SET(&ch, fd, EVFILT_READ | EVFILT_WRITE,
	       EV_ENABLE | EV_DISPATCH,
	       0, 0, this);
    }
}

};

