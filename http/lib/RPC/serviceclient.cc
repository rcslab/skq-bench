
#include <string>
#include <vector>
#include <memory>

#include <celestis/rpc/message.h>
#include <celestis/rpc/messagefilter.h>
#include <celestis/rpc/servicetransport.h>
#include <celestis/rpc/serviceclient.h>
#include <celestis/rpc/serializationexception.h>

#include "tcptransport.h"

namespace Celestis { namespace RPC {

void
ServiceClient::connect(const std::string &proto, const std::string &addr)
{
    if (proto == "TCP") {
	transport.reset(new TCPTransport());
	transport->connect(addr);
    }
}

void
ServiceClient::disconnect()
{
    transport->disconnect();
    transport.reset(nullptr);
}

void
ServiceClient::appendFilter(MessageFilter *filt)
{
    filters.push_back(filt);
}

void
ServiceClient::dispatchOOB(Message *in, Message *out)
{
    // Overloaded if there are OOB messages
}

void
ServiceClient::send(Message *in, Message *out)
{
    processOut(in);
    transport->send(in);
    transport->recv(out);
    processIn(out);
}

void
ServiceClient::sendAsync(Message *in)
{
    processOut(in);
    transport->send(in);
}

void
ServiceClient::dispatch()
{
    // read loop
}

void
ServiceClient::processIn(Message *msg)
{
    for (auto it = filters.rbegin(); it != filters.rend(); it++) {
	(*it)->processIn(msg);
    }
}

void
ServiceClient::processOut(Message *msg)
{
    for (auto it = filters.begin(); it != filters.end(); it++) {
	(*it)->processOut(msg);
    }
}

}; };

