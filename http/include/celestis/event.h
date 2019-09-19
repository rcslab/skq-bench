
#ifndef __CELESTIS_EVENT_H__
#define __CELESTIS_EVENT_H__

#include <sys/event.h>

#include <functional>

namespace Celestis {

class EventContext;

class Event
{
protected:
    Event(uintptr_t eventId = 0) : eventId(eventId) {}
public:
    virtual ~Event() {}
    virtual void cancel() = 0;
protected:
    virtual uintptr_t getId() { return eventId; }
    virtual void addEvent() = 0;
    virtual void delEvent() = 0;
    virtual void callback(EventContext &ctx, struct kevent &ev) = 0;
    uintptr_t eventId;
friend class EventLoop;
};

};

#endif /* __CELETIS_EVENT_H__ */

