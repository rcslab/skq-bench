
#ifndef __CELESTIS_EVENTLOOP_H__
#define __CELESTIS_EVENTLOOP_H__

#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>

#include <aio.h>
#include <sys/event.h>

#include <celestis/debug.h>

namespace Celestis {

class Event;
class EventTimer;
class EventSock;
class EventServerSock;
class EventAIO;

typedef std::function<void ()> DeferCB;
typedef std::function<void (int status, struct aiocb *)> AIOCB;

/*
 * Per-thread context used to hold event updates and deferred callbacks
 */
class EventContext
{
public:
    EventContext() : defer(), input() {
	for (int i = 0; i < aio_max; i++) {
	    iovp[i] = &iov[i];
	}
    }
    ~EventContext() = default;
    void appendDefer(DeferCB cb) {	
	defer.emplace_back(cb);
    }

    void appendWrite(int fd, void *buf, uint64_t sz, AIOCB cb) {
	int status;

	ASSERT(nio < aio_max);
	memset(&iov[nio], 0, sizeof(struct aiocb));
	iov[nio].aio_fildes = fd;
	iov[nio].aio_buf = buf;
	iov[nio].aio_nbytes = sz;
	iov[nio].aio_lio_opcode = LIO_WRITE;
	iov_cb[nio] = cb;
	nio++;

	if (nio >= aio_flush_point) {
	    status = lio_listio(LIO_WAIT, iovp, nio, NULL);
	    if (status == 0 || (status == -1 && errno == EIO)) {
		for (int i=0;i<nio;i++) {
		    iov_cb[i](status == 0? 0: aio_error(&iov[i]), &iov[i]);
		}
	    } else {
		perror("lio_listio()");
		abort();
	    }
	    nio = 0;
	}
    }
    struct kevent &alloc() {
	ASSERT(nev < event_max);
	nev++;
	return event[nev - 1];
    }

protected:
    std::vector<DeferCB> defer;
    std::unordered_map<int, std::string> input;
    friend class EventLoop;
    static const int event_max = 2048;
    int nev;
    struct kevent event[event_max];
    static const int aio_max = 256; /* Match FreeBSD default */
    static const int aio_flush_point = 2; /* Force aio list to flushe once nio equals to this number */
    int nio;
    struct aiocb iov[aio_max];
    struct aiocb *iovp[aio_max];
    AIOCB iov_cb[aio_max];
};

/*
 * Main event loop class
 */
class EventLoop
{
public:
    EventLoop();
    EventLoop(const EventLoop& rhs) = delete;
    EventLoop& operator=(const EventLoop& rhs) = delete;
    ~EventLoop();
    void enterLoop();
    void addEvent(Event *evt);
    void delEvent(Event *evt);
    void enableMkq(int mode) { /* Make sure you call this before enterLoop */
#ifdef FKQMULTI
	multikq = true; 
	if (ioctl(kq, FKQMULTI, &mode) < 0) {
	    WARN("Failed to enable KQMULTI mode!");
	    perror("ioctl");
	    abort();
	}
#else
	multikq = false;
#endif
    }
protected:
    bool multikq;
    int kq;
    friend class EventAIO;
    friend class EventServerSock;
    friend class EventSock;
    friend class EventTimer;
private:
    std::mutex lock;
    uintptr_t nextId;
    std::unordered_map<uintptr_t, Event*> events;
};

};

#endif /* __CELETIS_EVENTLOOP_H__ */

