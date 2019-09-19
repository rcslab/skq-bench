/*
 * Celestis::NetworkStream
 * Copyright (c) 2005-2018 Ali Mashtizadeh
 * All rights reserved.
 */

#include <cassert>

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

NetworkStream::NetworkStream()
{
    socket = NULL;
    ownsocket = false;
}

NetworkStream::NetworkStream(Socket *sock, bool owns)
{
    socket = sock;
    ownsocket = owns;
}

NetworkStream::~NetworkStream()
{
    if (ownsocket)
	delete socket;
    ownsocket = false;
}

void
NetworkStream::close()
{
    if (ownsocket)
	socket->close();
}

void
NetworkStream::flush()
{
}

size_t
NetworkStream::read(char *buf, off_t offset, size_t count)
{
    int retval = 0;

    assert(socket->isOpen() == true);

    retval = recv(socket->fd, buf + offset, count, 0);
    if (retval == -1) {
	throw std::system_error(errno, std::system_category());
    }
    if (retval >= 0)
        return retval;

    assert(false);
    return -1;
}

char
NetworkStream::readByte()
{
    int retval;
    char bite;

    assert(socket->isOpen() == true);

    retval = recv(socket->fd, &bite, 1, 0);
    if (retval == -1) {
	throw std::system_error(errno, std::system_category());
    }
    if (retval == 1)
        return bite;

    return -1;
}

off_t
NetworkStream::seek(off_t offset, int origin)
{
    throw std::exception();
}

void
NetworkStream::setLength(size_t value)
{
    throw std::exception();
}

void
NetworkStream::write(const char *buf, off_t offset, size_t count)
{
    int retval;

    assert(socket->isOpen() == true);

    retval = send(socket->fd, buf + offset, count, 0);
    if (retval == -1) {
	throw std::system_error(errno, std::system_category());
    }
    if (retval == 1)
	return;
}

void NetworkStream::writeByte(char value)
{
    int retval;

    assert(socket->isOpen() == true);

    retval = send(socket->fd, &value, 1, 0);
    if (retval == -1) {
	throw std::system_error(errno, std::system_category());
    }
    if (retval == 1)
        return;
}

bool
NetworkStream::canRead()
{
    return true;
}

bool
NetworkStream::canSeek()
{
    return false;
}

bool
NetworkStream::canWrite()
{
    return true;
}

}

