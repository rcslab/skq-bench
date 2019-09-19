
#ifndef __CELESTIS_EVENTSERVERSOCK_H__
#define __CELESTIS_EVENTSERVERSOCK_H__

#include <sys/event.h>

namespace Celestis {

class EventServerSock;

typedef std::function<void(EventServerSock&, int fd)> EventServerSockCB;

class EventServerSock : public Event
{
public:
    EventServerSock(EventLoop &el, EventServerSockCB cb, int fd);
    EventServerSock(const EventServerSock&) = delete;
    EventServerSock& operator=(const EventServerSock&) = delete;
    virtual ~EventServerSock();
    virtual void cancel();
protected:
    virtual void addEvent();
    virtual void delEvent();
    void callback(EventContext &ctx, struct kevent &ev);
friend class EventLoop;
private:
    bool enabled;
    int fd;
    EventLoop &el;
    EventServerSockCB cb;
};

};

#endif /* __CELETIS_EVENTSERVERSOCK_H__ */

