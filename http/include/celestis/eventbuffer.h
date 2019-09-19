
#ifndef __CELESTIS_EVENTBUFFER_H__
#define __CELESTIS_EVENTBUFFER_H__

#include <stdarg.h>

#include <array>
#include <queue>

namespace Celestis {

class EventBuffer;
class EventInputBuffer;
class EventOutputBuffer;

typedef std::function<void (EventContext&,EventBuffer&)> EventBufferCB;
typedef std::function<void (EventContext&,EventInputBuffer&)> EventInputBufferCB;
typedef std::function<void (EventContext&,EventOutputBuffer&)> EventOutputBufferCB;

struct EventBufferSegment
{
    EventBufferSegment() : offset(0), length(0), data(), fd(-1), off(0), sz(0) { }
    ~EventBufferSegment() { }
    static const int64_t BufferLength = 1500;
    int64_t offset;
    int64_t length;
    std::array<char, BufferLength> data;
    // Sendfile
    int fd;
    off_t off;
    size_t sz;
};

enum class EventBufferMode {
    ANY,
    EOL_CRLF,
    EOL_LF,
};

class EventInputBuffer
{
public:
    EventInputBuffer(EventSock &es, EventInputBufferCB cb);
    EventInputBuffer(const EventInputBuffer&) = delete;
    EventInputBuffer& operator=(const EventInputBuffer&) = delete;
    ~EventInputBuffer();
    void cancel();
    void read(char *buf, size_t sz);
    std::string readln(size_t maxSz = -1);
    void writefile(int fd, off_t off, size_t sz);
    size_t getLength();
    void setMode(EventBufferMode mode);
    void setCB(EventInputBufferCB cb);
    // XXX: Add error and timeout CBs
protected:
    friend class EventBuffer;
    int64_t scaneol();
    void readCB(EventContext &ctx, int64_t wrAvail);
private:
    EventBufferMode mode;
    EventSock &es;
    EventInputBufferCB cb;
    size_t totalLength;
    std::deque<EventBufferSegment> segs;
};

class EventOutputBuffer
{
public:
    EventOutputBuffer(EventSock &es, EventOutputBufferCB cb);
    EventOutputBuffer(const EventOutputBuffer&) = delete;
    EventOutputBuffer& operator=(const EventOutputBuffer&) = delete;
    ~EventOutputBuffer();
    void cancel();
    void write(const char *buf, size_t sz);
    void printf(const char *fmt, ...) __attribute__ ((__format__(printf, 2, 3)));
    void vprintf(const char *fmt, va_list ap) __attribute__ ((__format__(printf, 2, 0)));
    void sendfile(int fd, off_t off, size_t sz);
    void flush();
    void setCB(EventOutputBufferCB cb);
    // XXX: Add error and timeout CBs
protected:
    friend class EventBuffer;
    void writeCB(EventContext &ctx, int64_t wrAvail);
private:
    EventSock &es;
    EventContext *ctx; // Only valid inside callbacks
    EventOutputBufferCB cb;
    int64_t wrAvail;
    size_t totalLength;
    std::deque<EventBufferSegment> segs;
};

class EventBuffer
{
public:
    EventBuffer(EventSock &es, EventInputBufferCB icb, EventOutputBufferCB ocb);
    EventBuffer(const EventBuffer&) = delete;
    EventBuffer& operator=(const EventBuffer&) = delete;
    ~EventBuffer();
    void cancel();
    void write(const char *buf, size_t sz);
    void printf(const char *fmt, ...);
    void sendfile(int fd, off_t off, size_t sz);
    void flush();
    void read(char *buf, size_t sz);
    std::string readln(size_t maxSz = -1);
    void writefile(int fd, off_t off, size_t sz);
    size_t getLength();
    void setMode(EventBufferMode mode);
    void setInputCB(EventInputBufferCB cb);
    void setOutputCB(EventOutputBufferCB cb);
    void setErrorCB(EventBufferCB cb);
    // XXX: Add timeout CBs
private:
    void sockCB(EventContext &ctx, EventSock &es);
    void sockErrorCB(EventContext &ctx, EventSock &es);
    EventSock &es;
    EventInputBuffer ib;
    EventOutputBuffer ob;
    EventBufferCB ecb;
};
 
};

#endif /* __CELESTIS_EVENTBUFFER_H__ */

