/*
 * Celestis::Stream
 * Copyright (c) 2006-2018 Ali Mashtizadeh
 * All rights reserved.
 */

#ifndef __CELESTIS_STREAM_H__
#define __CELESTIS_STREAM_H__

namespace Celestis
{

class Stream
{
public:
    virtual ~Stream() { };
    virtual void close() = 0;
    virtual void flush() = 0;
    virtual size_t read(char *buf, off_t offset, size_t count) = 0;
    virtual char readByte() = 0;
    virtual off_t seek(off_t offset, int origin) = 0;
    virtual void setLength(size_t value) = 0;
    virtual void write(const char *buf, off_t offset, size_t count) = 0;
    virtual void writeByte(char value) = 0;
    virtual bool canRead() = 0;
    virtual bool canSeek() = 0;
    virtual bool canWrite() = 0;
};

}

#endif
