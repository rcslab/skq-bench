/*
 * Celestis::NetworkStream
 * Copyright (c) 2005-2018 Ali Mashtizadeh
 * All rights reserved.
 */

#ifndef __CELESTIS_NETWORKSTREAM_H__
#define __CELESTIS_NETWORKSTREAM_H__

namespace Celestis
{

class Socket;

class NetworkStream : public Stream
{
public:
    NetworkStream();
    NetworkStream(Socket *sock, bool owns = true);
    virtual ~NetworkStream();
    virtual void close();
    virtual void flush();
    virtual size_t read(char *buf, off_t offset, size_t count);
    virtual char readByte();
    virtual off_t seek(off_t offset, int origin); // Define Seek Origin
    virtual void setLength(size_t value);
    virtual void write(const char *buf, off_t offset, size_t count);
    virtual void writeByte(char value);
    //void sendFile(System::IO::FileStream &fs, int offset, int count);
    virtual bool canRead();
    virtual bool canSeek();
    virtual bool canWrite();
private:
    Socket *socket;
    bool ownsocket;
};

}

#endif
