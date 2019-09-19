
#ifndef __CELESTIS_EVENTSOCK_H__
#define __CELESTIS_EVENTSOCK_H__

#include <sys/event.h>

namespace Celestis {

class EventSock;
class EventOutputBuffer;
struct EventBufferSegment;

typedef std::function<void (EventContext&, EventSock&)> EventSockCB;

class EventSock : public Event
{
public:
    EventSock(EventLoop &el, EventSockCB cb, int fd);
    EventSock(const EventSock&) = delete;
    EventSock& operator=(const EventSock&) = delete;
    virtual ~EventSock();
    virtual void cancel();
    int read(void *buf, uint64_t sz);
    int write(const void *buf, uint64_t sz);
    int sendfile(int fd, off_t off, size_t sz);
    int sendseg(EventBufferSegment &seg);
    int getStatus() { return sockStatus; }
    int64_t readBytes() { return rdAvail; }
    int64_t writeBytes() { return wrAvail; }
    bool readShutdown() { return rdShutdown; }
    bool writeShutdown() { return wrShutdown; }
    void setFD(int fd) { this->fd = fd; }
    void setCB(EventSockCB cb) { this->cb = cb; }
    void setErrorCB(EventSockCB cb) { this->ecb = cb; }
protected:
    virtual void addEvent();
    virtual void delEvent();
    void callback(EventContext &ctx, struct kevent &ev);
friend class EventLoop;
friend class EventOutputBuffer;
private:
    bool enabled;
    bool rdShutdown;
    bool wrShutdown;
    int64_t rdAvail;
    int64_t wrAvail;
    int sockStatus;
    int fd;
    EventLoop &el;
    EventSockCB cb;
    EventSockCB ecb;
};

};

#endif /* __CELETIS_EVENTSOCK_H__ */

