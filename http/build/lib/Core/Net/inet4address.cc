/*
 * Network::Inet4Address
 * Copyright (c) 2005-2018 Ali Mashtizadeh
 * All rights reserved.
 */

#include <cstdint>
#include <cstring>

#include <sys/socket.h>
#include <netinet/in.h>

#include <string>
#include <sstream>
#include <stdexcept>

#include <celestis/inet4address.h>

namespace Celestis
{

Inet4Address::Inet4Address()
{
    IPAddress[0] = 0;
    IPAddress[1] = 0;
    IPAddress[2] = 0;
    IPAddress[3] = 0;
}

Inet4Address::Inet4Address(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    IPAddress[0] = a;
    IPAddress[1] = b;
    IPAddress[2] = c;
    IPAddress[3] = d;
}

Inet4Address::Inet4Address(uint16_t a, uint16_t b)
{
    IPAddress[0] = (a >> 8) & 0xFF;
    IPAddress[1] = a & 0xFF;
    IPAddress[2] = (b >> 8) & 0xFF;
    IPAddress[3] = b & 0xFF;
}

Inet4Address::Inet4Address(uint32_t ipaddr)
{
    IPAddress[0] = (ipaddr >> 24) & 0xFF;
    IPAddress[1] = (ipaddr >> 16) & 0xFF;
    IPAddress[2] = (ipaddr >> 8) & 0xFF;
    IPAddress[3] = ipaddr & 0xFF;
}

/*!
 * The following formats are supported:
 * X.X.X.X
 * XX.XX
 * XXXX
 */
Inet4Address::Inet4Address(const std::string &ipaddr)
{
    size_t i;
    uint32_t n = 0;
    uint32_t address[4];
    int term = 0;

    for (i = 0; i < ipaddr.size(); i++)
    {
	if (ipaddr[i] >= '0' && ipaddr[i] <= '9') {
	    n = (n * 10) + ipaddr[i];
	} else if (ipaddr[i] == '.') {
	    address[term] = n;
	    term++;
	    if (term > 3)
		throw std::invalid_argument("Inet4Address: Badly formatted IP Address.");
	    n = 0;
	} else {
	    throw std::invalid_argument("Inet4Address: Badly formatted IP Address.");
	}
    }

    switch (term)
    {
	case 1:
	    IPAddress[0] = (address[0] >> 24) & 0xFF;
	    IPAddress[1] = (address[0] >> 16) & 0xFF;
	    IPAddress[2] = (address[0] >> 8) & 0xFF;
	    IPAddress[3] = address[0] & 0xFF;
	    break;
	case 2:
	    IPAddress[0] = (address[0] >> 8) & 0xFF;
	    IPAddress[1] = address[0] & 0xFF;
	    IPAddress[2] = (address[1] >> 8) & 0xFF;
	    IPAddress[3] = address[1] & 0xFF;
	    break;
	case 4:
	    IPAddress[0] = address[0] & 0xFF;
	    IPAddress[1] = address[1] & 0xFF;
	    IPAddress[2] = address[2] & 0xFF;
	    IPAddress[3] = address[3] & 0xFF;
	    break;
	default:
	    throw std::invalid_argument("Inet4Address: Badly formatted IP Address.");
    }
}

Inet4Address::~Inet4Address()
{
}

InetAddress *
Inet4Address::clone() const
{
    return new Inet4Address(*this);
}

std::string Inet4Address::toString() const
{
    std::stringstream s;

    s << (int)IPAddress[0] << "." << (int)IPAddress[1]
      << "." << (int)IPAddress[2] << "." << (int)IPAddress[3];

    return s.str();
}

Inet4Address Inet4Address::Any()
{
    return Inet4Address(0,0,0,0);
}

Inet4Address Inet4Address::Broadcast()
{
    return Inet4Address(255,255,255,255);
}

Inet4Address Inet4Address::Loopback()
{
    return Inet4Address(127,0,0,1);
}

bool Inet4Address::isAny() const
{
    return (IPAddress[0] == 0) && (IPAddress[1] == 0) &&
	   (IPAddress[2] == 0) && (IPAddress[3] == 0);
}

bool Inet4Address::isBroadcast() const
{
    return (IPAddress[0] == 255) && (IPAddress[1] == 255) &&
	   (IPAddress[2] == 255) && (IPAddress[3] == 255);
}

bool Inet4Address::isLoopback() const
{
    return (IPAddress[0] == 127) && (IPAddress[1] == 0) &&
	   (IPAddress[2] == 0) && (IPAddress[3] == 1);
}

void Inet4Address::setSockAddr(struct sockaddr *addr)
{
    struct sockaddr_in *addrstruct = (struct sockaddr_in *)addr;
    uint32_t ipaddr = ntohl(addrstruct->sin_addr.s_addr);

    IPAddress[0] = ipaddr >> 24;
    IPAddress[1] = ipaddr >> 16;
    IPAddress[2] = ipaddr >> 8;
    IPAddress[3] = ipaddr;
}

void Inet4Address::getSockAddr(struct sockaddr *addr) const
{
    struct sockaddr_in *addrstruct = (struct sockaddr_in *)addr;

    memset((void *)addrstruct, 0, sizeof(*addrstruct));

    addrstruct->sin_family = AF_INET;
    addrstruct->sin_addr.s_addr = htonl((IPAddress[0] << 24) |
					(IPAddress[1] << 16) |
					(IPAddress[2] << 8) |
					IPAddress[3]);
}

}
