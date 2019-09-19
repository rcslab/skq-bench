
#ifndef __CELESTIS_EVENTAIO_H__
#define __CELESTIS_EVENTAIO_H__

#include <sys/event.h>
#include <aio.h>

namespace Celestis {

class EventAIO;

typedef void (*EventAIOCB)(EventAIO *);

class EventAIO : public Event
{
public:
    EventAIO(std::function<void(EventAIO*)> cb);
    virtual ~EventAIO();
    virtual void cancel();
    void read(int fd, void *buf, uint64_t off, uint64_t sz);
    void write(int fd, void *buf, uint64_t off, uint64_t sz);
protected:
    virtual void addEvent();
    virtual void delEvent();
    void callback(EventContext &ctx, struct kevent &ev);
friend class EventLoop;
private:
    uint64_t timeout;
    struct aiocb iocb;
    EventLoop *el;
    std::function<void(EventAIO*)> cb;
};

};

#endif /* __CELETIS_EVENTAIO_H__ */

