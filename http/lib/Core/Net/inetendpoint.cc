/*
 * Network::InetEndpoint
 * Copyright (c) 2005-2018 Ali Mashtizadeh
 * All rights reserved.
 */

#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#include <sys/socket.h>
#include <netinet/in.h>

#include <string>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <typeinfo>
#include <type_traits>

#include <celestis/inetaddress.h>
#include <celestis/inet4address.h>
#include <celestis/inetendpoint.h>

namespace Celestis
{

InetEndpoint::InetEndpoint()
    : hostname(), addr(nullptr), port(0)
{
}

InetEndpoint::InetEndpoint(const InetEndpoint &endp)
    : hostname(endp.hostname), addr(endp.addr->clone()), port(endp.port)
{
    update();
}


InetEndpoint::InetEndpoint(const InetAddress &addr, uint16_t port)
    : hostname(), addr(addr.clone()), port(port)
{
    update();
}

InetEndpoint::InetEndpoint(const std::string &addr, uint16_t port)
    : hostname(addr), addr(nullptr), port(port)
{
    // Name Resolution
}

InetEndpoint::InetEndpoint(uint16_t port)
    : hostname(), addr(nullptr), port(port)
{
}

InetEndpoint::~InetEndpoint()
{
}

InetEndpoint&
InetEndpoint::operator=(const InetEndpoint& rhs)
{
    addr.reset(rhs.addr->clone());
    port = rhs.port;
    update();

    return *this;
}

void
InetEndpoint::update()
{
    memset(&sockaddress, 0, sizeof(sockaddress));
    memset(&sockaddress6, 0, sizeof(sockaddress6));

    if (typeid(*addr) == typeid(Inet4Address)) {
	addr->getSockAddr((struct sockaddr *)&sockaddress);
	sockaddress.sin_port = htons(port);
    } else {
        assert(false);
    }
}

std::string
InetEndpoint::toString() const
{
    std::stringstream s;

    if (addr) {
	s << addr->toString() << ":" << port;
    } else {
	s << "null:" << port;
    }

    return s.str();
}

InetAddress &
InetEndpoint::getAddress() const
{
    return *addr;
}

std::string
InetEndpoint::getHostname() const
{
    return hostname;
}

uint16_t
InetEndpoint::getPort() const
{
    return port;
}

void
InetEndpoint::setSockAddr(struct sockaddr *inaddr)
{
    struct sockaddr_in *tmp = (struct sockaddr_in *)inaddr;

    if (tmp->sin_family == AF_INET) {
	memcpy(&sockaddress, inaddr, sizeof(struct sockaddr_in));

	port = ntohs(sockaddress.sin_port);

	addr.reset(new Inet4Address());
	addr->setSockAddr((struct sockaddr *)&sockaddress);
    } else {
	assert(false);
    }
}

struct sockaddr *
InetEndpoint::getSockAddr() const
{
    return (struct sockaddr *)&sockaddress;
}

int
InetEndpoint::getSockAddrLen() const
{
    if (sockaddress.sin_family == AF_INET)
    {
	return sizeof(sockaddress);
    } else if (sockaddress6.sin6_family == AF_INET6) {
	return sizeof(sockaddress6);
    } else {
	assert(false);
	return 0;
    }
}

}

