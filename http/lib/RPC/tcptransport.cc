
#include <cstring>

#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

#include <string>
#include <map>
#include <system_error>

#include <celestis/rpc/message.h>
#include <celestis/rpc/servicetransport.h>

#include "tcptransport.h"

namespace Celestis { namespace RPC {

TCPTransport::TCPTransport()
    : listenFd(-1), clientFd(-1)
{
}

TCPTransport::~TCPTransport()
{
}

void
TCPTransport::connect(const std::string &addr)
{
    int sock;
    int status;
    struct addrinfo hints, *res;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
	throw std::system_error(errno, std::system_category());

    int reuseAddr = 1;
    status = ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
			  &reuseAddr, sizeof(reuseAddr));
    if (status < 0) {
	::close(sock);
	throw std::system_error(errno, std::system_category());
    }

#ifdef __APPLE__
    status = ::setsockopt(sock, SOL_SOCKET, SO_REUSEPORT,
			  &reuseAddr, sizeof(reuseAddr));
    if (status < 0) {
	::close(sock);
	throw std::system_error(errno, std::system_category());
    }
#endif /* __APPLE__ */

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;
    status = getaddrinfo(addr.c_str(), "5555", &hints, &res);
    if (status) {
	throw std::system_error(status, std::generic_category(),
				gai_strerror(status));
    }

    status = ::connect(sock, res->ai_addr, res->ai_addrlen);
    if (status < 0) {
	::close(sock);
	throw std::system_error(errno, std::system_category());
    }

    clientFd = sock;
}

void
TCPTransport::disconnect()
{
    if (clientFd == -1) {
	::close(clientFd);
	clientFd = -1;
    }
}

void
TCPTransport::listen(const std::string &addr)
{
    int sock;
    int status;
    int len;
    struct sockaddr_in bindAddr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
	throw std::system_error(errno, std::system_category());

    int reuseAddr = 1;
    status = ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
			  &reuseAddr, sizeof(reuseAddr));
    if (status < 0) {
	::close(sock);
	throw std::system_error(errno, std::system_category());
    }

#ifdef __APPLE__
    status = ::setsockopt(sock, SOL_SOCKET, SO_REUSEPORT,
			  &reuseAddr, sizeof(reuseAddr));
    if (status < 0) {
	::close(sock);
	throw std::system_error(errno, std::system_category());
    }
#endif /* __APPLE__ */

    memset(&bindAddr, 0, sizeof(bindAddr));
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = 0;
    bindAddr.sin_port = htons(5555);

    len = sizeof(bindAddr);
    status = ::bind(sock, (struct sockaddr *)&bindAddr, len);
    if (status < 0) {
	::close(sock);
	throw std::system_error(errno, std::system_category());
    }

    status = ::listen(sock, 10);
    if (status < 0) {
	::close(sock);
	throw std::system_error(errno, std::system_category());
    }

    listenFd = sock;
}

void
TCPTransport::shutdown()
{
    if (listenFd != -1) {
	::close(listenFd);
	listenFd = -1;
    }
}

void
TCPTransport::accept()
{
    int client;
    socklen_t len;
    struct sockaddr_in remote;

    len = sizeof(sockaddr_in);
    client = ::accept(listenFd, (struct sockaddr *)&remote, &len);
    if (client < 0) {
	throw std::system_error(errno, std::system_category());
    }

    clientFd = client;
}

void
TCPTransport::close()
{
    if (clientFd != -1) {
	::close(clientFd);
	clientFd = -1;
    }
}

ServiceTransport::Status
TCPTransport::send(Message *msg)
{
    write(clientFd, msg->data(), msg->size());
    return ServiceTransport::OK;
}

ServiceTransport::Status
TCPTransport::readExact(uint8_t *data, size_t len)
{
    int status;

    while (len > 0) {
	status = read(clientFd, data, len);
	if (status == 0) {
	    return ServiceTransport::DISCONNECT;
	}
	if (status < 0) {
	    throw std::system_error(errno, std::system_category());
	}

	data += status;
	len -= status;
    }

    return ServiceTransport::OK;
}

ServiceTransport::Status
TCPTransport::recv(Message *msg)
{
    ServiceTransport::Status status;
    uint8_t *header = msg->data();
    uint32_t msglen;

    status = readExact(header, Message::HEADER_SIZE);
    if (status != ServiceTransport::OK)
	return status;

    msglen = msg->prepareBuffer();

    return readExact(header+8, msglen - 8);
}

}; };

