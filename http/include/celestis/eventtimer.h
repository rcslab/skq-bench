
#ifndef __CELESTIS_EVENTTIMER_H__
#define __CELESTIS_EVENTTIMER_H__

#include <sys/event.h>

namespace Celestis {

class EventTimer;

typedef std::function<void(EventTimer&)> EventTimerCB;

class EventTimer : public Event
{
public:
    EventTimer(EventLoop &el, EventTimerCB cb, uintptr_t eventId = ULONG_MAX);
    EventTimer(const EventTimer&) = delete;
    EventTimer& operator=(const EventTimer&) = delete;
    virtual ~EventTimer();
    virtual void cancel();
    void oneshot(uint64_t ms);
    void periodic(uint64_t ms);
protected:
    virtual void addEvent();
    virtual void delEvent();
    virtual void callback(EventContext &ctx, struct kevent &ev);
friend class EventLoop;
private:
    bool isOneshot;
    uint64_t timeout;
    EventLoop &el;
    EventTimerCB cb;
};

};

#endif /* __CELETIS_EVENTTIMER_H__ */

