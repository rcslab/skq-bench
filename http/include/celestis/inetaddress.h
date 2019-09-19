/*
 * Network::InetAddress
 * Copyright (c) 2005-2018 Ali Mashtizadeh
 * All rights reserved.
 */

#ifndef __CELESTIS_INETADDRESS_H__
#define __CELESTIS_INETADDRESS_H__

namespace Celestis
{

class InetAddress
{
protected:
    InetAddress() {}
public:
    virtual ~InetAddress() {}
    virtual InetAddress *clone() const = 0;
    virtual std::string toString() const = 0;
    virtual bool isAny() const = 0;
    virtual bool isBroadcast() const = 0;
    virtual bool isLoopback() const = 0;
    virtual void setSockAddr(struct sockaddr *addr) = 0;
    virtual void getSockAddr(struct sockaddr *addr) const = 0;
};

}

#endif
