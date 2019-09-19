
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
#include <celestis/datagramsocket.h>

namespace Celestis
{

DatagramSocket::DatagramSocket()
    : sock(-1), open(false), bound(false), laddr()
{
    sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

DatagramSocket::~DatagramSocket()
{
    if (open)
	close();
}

void
DatagramSocket::close()
{
    ::close(sock);
    sock = -1;
    open = false;
}

void
DatagramSocket::bind(const InetEndpoint &endp)
{
    laddr = endp;

    if (::bind(sock,
	       laddr.getSockAddr(),
	       laddr.getSockAddrLen()) < 0) {
	throw std::system_error(errno, std::system_category());
    }

    bound = true;

    return;
}

void
DatagramSocket::bind(uint16_t port, const InetAddress &addr)
{
    bind(InetEndpoint(addr, port));
}

void
DatagramSocket::send(const InetEndpoint &endp, const char *buf, size_t len)
{
    int status;

    status = ::sendto(sock, buf, len, 0, endp.getSockAddr(), endp.getSockAddrLen());
    if (status == -1) {
	throw std::system_error(errno, std::system_category());
    }
}

void
DatagramSocket::recv(InetEndpoint *endp, char *buf, size_t *len)
{
    int status;
    union {
	struct sockaddr_in inaddr4;
	struct sockaddr_in6 inaddr6;
    } inaddr;
    socklen_t inaddrlen = sizeof(inaddr);

    status = ::recvfrom(sock, buf, *len, 0,
			(struct sockaddr *)&inaddr, &inaddrlen);
    if (status == -1) {
	throw std::system_error(errno, std::system_category());
    }

    endp->setSockAddr((sockaddr *)&inaddr);

    *len = status;
}

}

