/*
 * Celestis::ServerSocket
 * Copyright (c) 2005-2018 Ali Mashtizadeh
 * All rights reserved.
 *
 */

#ifndef __CELESTIS_SERVERSOCKET_H__
#define __CELESTIS_SERVERSOCKET_H__

#define DEFAULT_BACKLOG 10

namespace Celestis
{

class ServerSocket {
public:
    ServerSocket();
    ServerSocket(uint16_t port, uint32_t backlog = DEFAULT_BACKLOG);
    ServerSocket(const InetEndpoint &endp, uint32_t backlog = DEFAULT_BACKLOG);
    ~ServerSocket();
    void bind(uint16_t port, uint32_t backlog = DEFAULT_BACKLOG);
    void bind(const InetEndpoint &endp, uint32_t backlog = DEFAULT_BACKLOG);
    void open();
    void close();
    Socket accept();
    bool isOpen();
    bool isBound();
    uint16_t getLocalPort();
    InetAddress &getInetAddress();
    InetEndpoint getEndpoint();
    void setTimeout(uint64_t timeout);
    uint64_t getTimeout();
    void setReceiveBufferSize(size_t size);
    size_t getReceiveBufferSize();
    void setReuseAddress(bool onoff);
    bool getReuseAddress();
    void setReusePort(bool onoff);
    bool getReusePort();
private:
    InetEndpoint localaddress;
    bool isopen;
    bool isbound;
    int fd;
};

}

#endif
