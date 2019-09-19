/*
 * Celestis::BufferedStream
 * Copyright (c) 2006-2018 Ali Mashtizadeh
 * All rights reserved.
 */

#ifndef __CELESTIS_BUFFEREDSTREAM_H__
#define __CELESTIS_BUFFEREDSTREAM_H__

namespace Celestis
{

class BufferedStream : public Stream
{
public:
    BufferedStream();
    BufferedStream(const BufferedStream &b);
    BufferedStream(Stream *ios, int inputbuflen = 1024, int outputbuflen = 1024);
    virtual ~BufferedStream();
    BufferedStream &operator=(const BufferedStream &rhs);
    virtual void close();
    virtual void flush();
    virtual size_t read(char *buf, off_t offset, size_t count);
    virtual char readByte();
    virtual size_t readLine(char *buf, size_t bufLength);
    virtual std::string readLine(size_t maxLength);
    virtual off_t seek(off_t offset, int origin);
    virtual void setLength(size_t value);
    virtual void write(const char *buf, off_t offset, size_t count);
    virtual void writeByte(char value);
    virtual void writeLine(const std::string &str);
    virtual bool canRead();
    virtual bool canSeek();
    virtual bool canWrite();
private:
    Stream *io;
    size_t inputBufferLength;
    size_t inputBufferCur;
    char *inputBuffer;
    size_t outputBufferLength;
    size_t outputBufferCur;
    char *outputBuffer;
};

}

#endif
