/*
 * Celestis::ServerSocket
 * Copyright (c) 2005-2018 Ali Mashtizadeh
 * All rights reserved.
 */

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string>
#include <memory>
#include <system_error>

#include <celestis/inetaddress.h>
#include <celestis/inet4address.h>
#include <celestis/inetendpoint.h>
#include <celestis/stream.h>
#include <celestis/networkstream.h>
#include <celestis/socket.h>
#include <celestis/serversocket.h>

namespace Celestis
{

ServerSocket::ServerSocket()
{
    isopen = false;
    isbound = false;
    open();
}

ServerSocket::ServerSocket(uint16_t port, uint32_t backlog)
{
    isopen = false;
    isbound = false;
    open();
    bind(port, backlog);
}

ServerSocket::ServerSocket(const InetEndpoint &endp, uint32_t backlog)
{
    isopen = false;
    isbound = false;
    open();
    bind(endp, backlog);
}

ServerSocket::~ServerSocket()
{
    if (isopen)
	close();
}

void ServerSocket::bind(uint16_t port, uint32_t backlog) {
    struct sockaddr *addrstruct;
    socklen_t addrstructlen;

    assert(isbound != true);

    localaddress = InetEndpoint(Inet4Address::Any(), port);

    addrstruct = localaddress.getSockAddr();
    addrstructlen = localaddress.getSockAddrLen();

    open();

    if (::bind(fd, (struct sockaddr *)addrstruct, addrstructlen) < 0)
	throw std::system_error(errno, std::system_category());

    if (listen(fd, backlog) < 0)
	throw std::system_error(errno, std::system_category());
    isbound = true;
}

void ServerSocket::bind(const InetEndpoint &endp, uint32_t backlog) {
    struct sockaddr *addrstruct;
    socklen_t addrstructlen;
            
    assert(isbound != true);

    localaddress = InetEndpoint(endp.getAddress(), endp.getPort());

    addrstruct = localaddress.getSockAddr();
    addrstructlen = localaddress.getSockAddrLen();

    open();

    if (::bind(fd, (struct sockaddr *)&addrstruct, addrstructlen) < 0)
	throw std::system_error(errno, std::system_category());

    if (::listen(fd, backlog) < 0)
	throw std::system_error(errno, std::system_category());
    isbound = true;
}

void ServerSocket::open() {
    if (!isopen) {
	fd = socket(PF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
	    throw std::system_error(errno, std::system_category());
	}
	isopen = true;
    }
}

void ServerSocket::close() {
    if (isopen)
        ::close(fd);

    isopen = false;
    isbound = false;

    return;
}

Socket ServerSocket::accept() {
    struct sockaddr_in addrstruct;
    int addrlen = sizeof(addrstruct);

    memset(&addrstruct, 0, addrlen);

    int sockhand = ::accept(fd, (struct sockaddr *)&addrstruct, (socklen_t *)&addrlen);
    if (sockhand < 0)
	throw std::system_error(errno, std::system_category());

    //inet_ntop(AF_INET, &addrstruct.sin_addr, s->remoteaddr, INET_ADDRSTRLEN);

    return Socket(sockhand);
}

bool ServerSocket::isOpen() {
    return isopen;
}

bool ServerSocket::isBound() {
    return isbound;
}

uint16_t ServerSocket::getLocalPort() {
    return localaddress.getPort();
}

InetAddress &ServerSocket::getInetAddress() {
    return localaddress.getAddress();
}

InetEndpoint ServerSocket::getEndpoint() {
    return localaddress;
}

void ServerSocket::setTimeout(uint64_t timeout) {
    struct timeval timer;

    timer.tv_sec = timeout / 1000;
    timer.tv_usec = (timeout % 1000) * 1000;

    setsockopt(fd,SOL_SOCKET, SO_RCVTIMEO, &timer, sizeof(timer));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timer, sizeof(timer));
}

uint64_t ServerSocket::getTimeout() {
    struct timeval timer;
    socklen_t optionlen = sizeof(timer);

    getsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timer, &optionlen);

    return (timer.tv_sec * 1000) + (timer.tv_usec / 1000);
}

void ServerSocket::setReceiveBufferSize(size_t size) {
    size_t buffersize;

    setsockopt(fd,SOL_SOCKET, SO_SNDBUF, &buffersize, sizeof(buffersize));
}

size_t ServerSocket::getReceiveBufferSize() {
    size_t buffersize;
    socklen_t optionlen = sizeof(buffersize);

    getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buffersize, &optionlen);

    return buffersize;
}

void ServerSocket::setReuseAddress(bool onoff) {
    int optval = onoff ? 1 : 0;

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

bool ServerSocket::getReuseAddress() {
    int optval;
    socklen_t optlen = sizeof(optval);

    getsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, &optlen);

    return (optval == 1);
}

void ServerSocket::setReusePort(bool onoff) {
    int optval = onoff ? 1 : 0;

    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}

bool ServerSocket::getReusePort() {
    int optval;
    socklen_t optlen = sizeof(optval);

    getsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &optval, &optlen);

    return (optval == 1);
}

}

