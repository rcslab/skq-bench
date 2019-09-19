
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include <sys/signal.h>

#include <string>
#include <array>
#include <vector>
#include <algorithm>
#include <functional>

#include <celestis/debug.h>
#include <celestis/eventloop.h>
#include <celestis/event.h>
#include <celestis/eventsock.h>
#include <celestis/eventbuffer.h>

namespace Celestis {

EventInputBuffer::EventInputBuffer(EventSock &es, EventInputBufferCB cb)
    : es(es), cb(cb), totalLength(0), segs()
{
}

EventInputBuffer::~EventInputBuffer()
{
}

void
EventInputBuffer::cancel()
{
    es.cancel();
}

void
EventInputBuffer::read(char *buf, size_t sz)
{
    ASSERT(sz <= totalLength);

    for (auto &s : segs) {
	int64_t chunk = std::min((int64_t)sz, s.length);

	memcpy(buf, &s.data[s.offset], chunk);
	s.offset += chunk;
	s.length -= chunk;
	totalLength -= chunk;
	if (s.length == 0) {
	    segs.pop_front();
	}

	buf += chunk;
	sz -= chunk;
	if (sz == 0)
	    break;
    }
}

std::string
EventInputBuffer::readln(size_t maxSz)
{
    std::string buf;
    int64_t len = scaneol();
    int64_t off = 0;

    if (len < 0)
	return "";
    // XXX: Ignore maxSz for now

    buf.resize(len);
    for (auto &s : segs) {
	int64_t chunk = std::min(len, s.length);

	memcpy(&buf[off], &s.data[s.offset], chunk);
	s.offset += chunk;
	s.length -= chunk;
	off += chunk;
	totalLength -= chunk;
	if (s.length == 0) {
	    segs.pop_front();
	}
    }

    if (mode == EventBufferMode::EOL_CRLF) {
	buf.resize(buf.size() - 2);
    } else if (mode == EventBufferMode::EOL_LF) {
	buf.resize(buf.size() - 1);
    }

    return buf;
}

void
EventInputBuffer::writefile(int fd, off_t off, size_t sz)
{
    NOT_IMPLEMENTED();
}

size_t
EventInputBuffer::getLength()
{
    return totalLength;
}

void
EventInputBuffer::setMode(EventBufferMode mode)
{
    this->mode = mode;
}

void
EventInputBuffer::setCB(EventInputBufferCB cb)
{
    this->cb = cb;
}

int64_t
EventInputBuffer::scaneol()
{
    bool cr = false;
    int64_t length = 0;

    // Scan for CRLF
    for (auto &&s : segs) {
	for (int i = 0; i < s.length; i++) {
	    if (mode == EventBufferMode::EOL_CRLF) {
		if (cr) {
		    if (s.data[s.offset+i] == '\n') {
			return length + i + 1;
		    }
		}
		cr = (s.data[s.offset+i] == '\r');
	    }
	    if (mode == EventBufferMode::EOL_LF &&
		s.data[s.offset+i] == '\n') {
		return length + i + 1;
	    }
	}
	length += s.length;
    }

    return -1;
}

void
EventInputBuffer::readCB(EventContext &ctx, int64_t rdAvail)
{
    // Read data in
    if (!segs.empty()) {
	EventBufferSegment &s = segs.back();
	int64_t avail = s.data.max_size() - s.offset - s.length;
	int64_t tocopy = std::min(avail, rdAvail);

	es.read(&s.data[s.offset+s.length], tocopy);
	rdAvail -= tocopy;
	totalLength += tocopy;
    }
    while (rdAvail > 0) {
	segs.emplace_back();
	EventBufferSegment &s = segs.back();
	int64_t tocopy = std::min((int64_t)s.data.max_size(), rdAvail);

	es.read(&s.data[s.offset+s.length], tocopy);
	s.length += tocopy;
	rdAvail -= tocopy;
	totalLength += tocopy;
    }

    // Make callbacks
    if (mode == EventBufferMode::ANY) {
	cb(ctx, *this);
    } else {
	uint64_t oldLength = totalLength;
	while (1) {
	    if (scaneol() >= 0)
		cb(ctx, *this);
	    else
		break;

	    // If the callback hasn't read data then loop
	    if (oldLength == totalLength)
		break;
	}
    }
}

EventOutputBuffer::EventOutputBuffer(EventSock &es, EventOutputBufferCB cb)
    : es(es), ctx(nullptr), cb(cb), wrAvail(0), totalLength(0), segs()
{
    /*
     * XXX: This is a hack to ensure we think there is some space until we get 
     * a callback.
     */
    wrAvail = 1024;
}

EventOutputBuffer::~EventOutputBuffer()
{
}

void
EventOutputBuffer::cancel()
{
    es.cancel();
}

void
EventOutputBuffer::write(const char *buf, size_t sz)
{
    // Read data in
    if (!segs.empty() && segs.back().fd == -1) {
	EventBufferSegment &s = segs.back();
	int64_t avail = s.data.max_size() - s.offset - s.length;
	int64_t tocopy = std::min(avail, (int64_t)sz);

	memcpy(&s.data[s.offset+s.length], buf, tocopy);
	s.length += tocopy;
	totalLength += tocopy;

	buf += tocopy;
	sz -= tocopy;
    }
    while (sz > 0) {
	segs.emplace_back();
	EventBufferSegment &s = segs.back();
	int64_t tocopy = std::min(s.data.max_size(), sz);

	memcpy(&s.data[0], buf, tocopy);
	s.length += tocopy;
	totalLength += tocopy;

	buf += tocopy;
	sz -= tocopy;
    }
}

void
EventOutputBuffer::printf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

void
EventOutputBuffer::vprintf(const char *fmt, va_list ap)
{
    int len;
    char buf[512];

    // XXX: Needs more error checking here
    len = vsnprintf(buf, 512, fmt, ap);

    write(buf, len);
}

void
EventOutputBuffer::sendfile(int fd, off_t off, size_t sz)
{
    if (segs.empty() || segs.back().fd != -1) {
	segs.emplace_back();
    }

    EventBufferSegment &s = segs.back();
#if 0
    s.fd = fd;
    s.off = off;
    s.sz = sz;
#else
    int64_t avail = s.data.max_size() - s.offset - s.length;
    if (avail > sz) {
	int status = ::read(fd, &s.data[s.offset+s.length], sz);
	if (status < 0) {
	    perror("read()");
	    abort();
	}
	s.length += sz;
	totalLength += sz;
    } else {
	s.fd = fd;
	s.off = off;
	s.sz = sz;
    }
#endif
}

void
EventOutputBuffer::flush()
{
    for (auto &s : segs) {
	EventContext *ctx = this->ctx;

	if (segs.size() == 1 && s.fd == -1 && ctx != nullptr) {
	    ctx->appendWrite(es.fd, &s.data[s.offset], s.length,
			    [this](int status, struct aiocb *aio){
				if (status == 0) {
				    this->totalLength = 0;
				    this->segs.pop_front();
				} else {
				    perror("aio_error()");
				    // XXX: Call error CB
				}
			    });
	} else {
	    totalLength -= es.sendseg(s);

	    segs.pop_front();
	}
    }
}

void
EventOutputBuffer::setCB(EventOutputBufferCB cb)
{
    this->cb = cb;
}

void
EventOutputBuffer::writeCB(EventContext &ctx, int64_t wrAvail)
{
    // Update space
    this->wrAvail = wrAvail;
    this->ctx = &ctx;

    flush();

    if (totalLength == 0)
	cb(ctx, *this);
    
    this->ctx = nullptr;
}

EventBuffer::EventBuffer(EventSock &es, EventInputBufferCB icb, EventOutputBufferCB ocb)
    : es(es), ib(es, icb), ob(es, ocb)
{
    es.setCB([this](EventContext &ctx, EventSock &s){ this->sockCB(ctx, s); });
    es.setErrorCB([this](EventContext &ctx, EventSock &s){ this->sockErrorCB(ctx, s); });
}

EventBuffer::~EventBuffer()
{
}

void
EventBuffer::cancel()
{
    es.cancel();
}

void
EventBuffer::write(const char *buf, size_t sz)
{
    ob.write(buf, sz);
}

void
EventBuffer::printf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    ob.vprintf(fmt, ap);
    va_end(ap);
}

void
EventBuffer::sendfile(int fd, off_t off, size_t sz)
{
    ob.sendfile(fd, off, sz);
}

void
EventBuffer::flush()
{
    ob.flush();
}

void
EventBuffer::read(char *buf, size_t sz)
{
    ib.read(buf, sz);
}

std::string
EventBuffer::readln(size_t maxSz)
{
    return ib.readln(maxSz);
}

void
EventBuffer::writefile(int fd, off_t off, size_t sz)
{
    ib.writefile(fd, off, sz);
}

size_t
EventBuffer::getLength()
{
    return ib.getLength();
}

void
EventBuffer::setMode(EventBufferMode mode)
{
    ib.setMode(mode);
}

void
EventBuffer::setInputCB(EventInputBufferCB cb)
{
    ib.setCB(cb);
}

void
EventBuffer::setOutputCB(EventOutputBufferCB cb)
{
    ob.setCB(cb);
}

void
EventBuffer::setErrorCB(EventBufferCB cb)
{
    ecb = cb;
}

void
EventBuffer::sockCB(EventContext &ctx, EventSock &es)
{
    if (es.readBytes() > 0) {
	ib.readCB(ctx, es.readBytes());
    }
    if (es.writeBytes() > 0) {
	ob.writeCB(ctx, es.writeBytes());
    }
}

void
EventBuffer::sockErrorCB(EventContext &ctx, EventSock &es)
{
    ecb(ctx, *this);
}

};

