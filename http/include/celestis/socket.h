/*
 * Network::Socket
 * Copyright (c) 2005-2018 Ali Mashtizadeh
 * All rights reserved.
 */

#ifndef __CELESTIS_SOCKET_H__
#define __CELESTIS_SOCKET_H__

namespace Celestis
{

class Socket
{
public:
    Socket();
    Socket(int protofamily, int socktype, int prototype);
    ~Socket();
    void bind(const InetAddress &addr, uint16_t port);
    void bind(const InetEndpoint &endp);
    void connect(const InetAddress &addr, uint16_t port);
    void connect(const InetEndpoint &endp);
    void close();
    NetworkStream &getStream();
    uint16_t getLocalPort();
    InetAddress &getLocalAddress();
    InetEndpoint getLocalEndpoint();
    uint16_t getPort();
    InetAddress &getAddress();
    InetEndpoint getEndpoint();
    bool isOpen();
    bool isBound();
    bool isConnected();
    // Socket Options
    void setTimeout(size_t timeout);
    size_t getTimeout();
    void setSendBufferSize(size_t size);
    size_t getSendBufferSize();
    void setReceiveBufferSize(size_t size);
    size_t getReceiveBufferSize();
    void setReuseAddress(bool onoff);
    bool getReuseAddress();
    void setReusePort(bool onoff);
    bool getReusePort();
    void setOOBInline(bool onoff);
    bool getOOBInline();
    void setKeepAlive(bool onoff);
    bool getKeepAlive();
protected:
    int fd;
    Socket(int sock);
private:
    InetEndpoint localaddress;
    InetEndpoint address;
    NetworkStream *netstream;
    bool open;
    bool bound;
    bool connected;
    friend class NetworkStream;
    friend class ServerSocket;
};

}

#endif
