/*
 * Network::Inet4Address
 * Copyright (c) 2005-2018 Ali Mashtizadeh
 * All rights reserved.
 */

#ifndef __CELESTIS_INET4ADDRESS_H__
#define __CELESTIS_INET4ADDRESS_H__

#include <celestis/inetaddress.h>

namespace Celestis
{

class Inet4Address : public InetAddress
{
public:
    Inet4Address();
    Inet4Address(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
    Inet4Address(uint16_t a, uint16_t b);
    Inet4Address(uint32_t addr);
    Inet4Address(const std::string &ipaddr);
    virtual ~Inet4Address();
    virtual InetAddress *clone() const;
    virtual std::string toString() const;
    static Inet4Address Any();
    static Inet4Address Broadcast();
    static Inet4Address Loopback();
    virtual bool isAny() const;
    virtual bool isBroadcast() const;
    virtual bool isLoopback() const;
    virtual void setSockAddr(struct sockaddr *addr);
    virtual void getSockAddr(struct sockaddr *addr) const;
private:
    uint8_t IPAddress[4];
};

}

#endif
