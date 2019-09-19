/*
 * Network::InetEndpoint
 * Copyright (c) 2005-2018 Ali Mashtizadeh
 * All rights reserved.
 */

#ifndef __CELESTIS_INETENDPOINT_H__
#define __CELESTIS_INETENDPOINT_H__

#include <sys/socket.h>
#include <netinet/in.h>

namespace Celestis
{

class InetEndpoint
{
public:
    InetEndpoint();
    InetEndpoint(const InetEndpoint &endp);
    InetEndpoint(const InetAddress &addr, uint16_t port);
    InetEndpoint(const std::string &addr, uint16_t port);
    InetEndpoint(uint16_t port);
    virtual ~InetEndpoint();
    InetEndpoint& operator=(const InetEndpoint& rhs);
    std::string toString() const;
    InetAddress &getAddress() const;
    std::string getHostname() const;
    uint16_t getPort() const;
    void setSockAddr(struct sockaddr *addr);
    struct sockaddr *getSockAddr() const;
    int getSockAddrLen() const;
private:
    void update();
    std::string hostname;
    std::unique_ptr<InetAddress> addr;
    uint16_t port;
    union {
	struct sockaddr_in sockaddress;
	struct sockaddr_in6 sockaddress6;
    };
};

}

#endif
