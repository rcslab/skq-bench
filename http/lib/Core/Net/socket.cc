/*
 * Celestis::Socket
 * Copyright (c) 2005-2018 Ali Mashtizadeh
 * All rights reserved.
 */

#include <cstdint>

#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string>
#include <memory>
#include <system_error>

#include <celestis/inetaddress.h>
#include <celestis/inetendpoint.h>
#include <celestis/stream.h>
#include <celestis/networkstream.h>
#include <celestis/socket.h>

namespace Celestis
{

Socket::Socket()
    : localaddress(), address()
{
    fd = 0;
    netstream = 0;
    open = false;
}

Socket::Socket(int sock)
    : localaddress(), address()
{
    fd = sock;
    netstream = 0;
    open = true; // Assume its open

    struct sockaddr_in saddr;
    socklen_t addrlen = sizeof(sockaddr_in);
    if (::getpeername(fd, (struct sockaddr *)&saddr, &addrlen) < 0)
	throw std::system_error(errno, std::system_category());
    address.setSockAddr((struct sockaddr *)&saddr);
    if (::getsockname(fd, (struct sockaddr *)&saddr, &addrlen) < 0)
	throw std::system_error(errno, std::system_category());
    localaddress.setSockAddr((struct sockaddr *)&saddr);
}

Socket::~Socket()
{
    netstream = 0;

    if (open)
	close();
}

void Socket::bind(const InetAddress &addr, uint16_t port)
{
    bind(InetEndpoint(addr, port));
}

void Socket::bind(const InetEndpoint &endp)
{
    localaddress = endp;

    struct sockaddr *addrstruct = localaddress.getSockAddr();
    int addrlen = localaddress.getSockAddrLen();

    fd = ::socket(PF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
	throw std::system_error(errno, std::system_category());
    }
    open = true;

    if (::bind(fd,(struct sockaddr *)addrstruct, addrlen) < 0) {
	throw std::system_error(errno, std::system_category());
    }
}

void Socket::connect(const InetAddress &addr, uint16_t port)
{
    connect(InetEndpoint(addr, port));
}

void Socket::connect(const InetEndpoint &endp)
{
    address = endp;

    struct sockaddr *addrstruct = address.getSockAddr();
    int addrlen = localaddress.getSockAddrLen();

    if (::connect(fd, (struct sockaddr *)addrstruct, addrlen) < 0) {
	throw std::system_error(errno, std::system_category());
    }
}

void Socket::close() {
    if (netstream != 0)
	netstream->close();

    if (open) {
	if (::close(fd) < 0)
	    throw std::system_error(errno, std::system_category());

	open = false;
    }
}

NetworkStream &Socket::getStream()
{
    if (netstream == nullptr) {
	netstream = new NetworkStream(this, false);
    }

    return *netstream;
}

uint16_t Socket::getLocalPort()
{
    return localaddress.getPort();
}

InetAddress &Socket::getLocalAddress()
{
    return localaddress.getAddress();
}

InetEndpoint Socket::getLocalEndpoint()
{
    return localaddress;
}

uint16_t Socket::getPort() {
    return address.getPort();
}

InetAddress &Socket::getAddress() {
    return address.getAddress();
}

InetEndpoint Socket::getEndpoint() {
    return address;
}

bool Socket::isOpen() {
    return open;
}

bool Socket::isBound() {
    return bound;
}

bool Socket::isConnected() {
    return connected;
}

void Socket::setTimeout(size_t timeout) {
    struct timeval timer;

    timer.tv_sec = timeout / 1000;
    timer.tv_usec = (timeout % 1000) * 1000;

    setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timer,sizeof(timer));
    setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timer,sizeof(timer));
}

size_t Socket::getTimeout() {
    struct timeval timer;
    socklen_t optionlen = sizeof(timer);

    getsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timer,&optionlen);

    return (timer.tv_sec * 1000) + (timer.tv_usec / 1000);
}

void Socket::setSendBufferSize(size_t size) {
    size_t buffersize = size;

    setsockopt(fd,SOL_SOCKET,SO_SNDBUF,&buffersize,sizeof(buffersize));
}

size_t Socket::getSendBufferSize() {
    size_t buffersize;
    socklen_t optionlen = sizeof(buffersize);

    getsockopt(fd,SOL_SOCKET,SO_SNDBUF,&buffersize,&optionlen);
    return buffersize;
}

void Socket::setReceiveBufferSize(size_t size) {
    size_t buffersize = size;

    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buffersize, sizeof(buffersize));
}

size_t Socket::getReceiveBufferSize() {
    size_t buffersize;
    socklen_t optionlen = sizeof(buffersize);

    getsockopt(fd,SOL_SOCKET,SO_RCVBUF,&buffersize,&optionlen);

    return buffersize;
}

void Socket::setReuseAddress(bool onoff) {
    bool reuseaddress = onoff;

    setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&reuseaddress,sizeof(reuseaddress));
}

bool Socket::getReuseAddress() {
    bool reuseaddress;
    socklen_t optionlen = sizeof(reuseaddress);

    getsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddress, &optionlen);

    return reuseaddress;
}

void Socket::setReusePort(bool onoff) {
    bool reuseport = onoff;

    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &reuseport, sizeof(reuseport));
}

bool Socket::getReusePort() {
    bool reuseport;
    socklen_t optionlen = sizeof(reuseport);

    getsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &reuseport, &optionlen);

    return reuseport;
}

void Socket::setOOBInline(bool onoff) {
    bool oobinline = onoff;

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &oobinline, sizeof(oobinline));
}

bool Socket::getOOBInline() {
    bool oobinline;
    socklen_t optionlen = sizeof(oobinline);

    getsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &oobinline, &optionlen);

    return oobinline;
}

void Socket::setKeepAlive(bool onoff) {
    bool keepalive = onoff;

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &keepalive, sizeof(keepalive));
}

bool Socket::getKeepAlive() {
    bool keepalive;
    socklen_t optionlen = sizeof(keepalive);

    getsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &keepalive, &optionlen);

    return keepalive;
}

}
