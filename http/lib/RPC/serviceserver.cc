
#include <string>
#include <vector>

#include <celestis/rpc/message.h>
#include <celestis/rpc/messagefilter.h>
#include <celestis/rpc/servicetransport.h>
#include <celestis/rpc/serviceserver.h>
#include <celestis/rpc/serializationexception.h>

#include "tcptransport.h"

namespace Celestis { namespace RPC {

ServiceServer::ServiceServer()
    : stopRequested(false), filters(), transport(nullptr)
{
}

ServiceServer::~ServiceServer()
{
}

void
ServiceServer::appendFilter(MessageFilter *filt)
{
    filters.push_back(filt);
}

void
ServiceServer::start(const std::string &proto, const std::string &addr)
{
    if (proto == "TCP") {
	transport.reset(new TCPTransport());
	transport->listen(addr);
    }

    dispatch();
}

void
ServiceServer::stop()
{
    stopRequested.store(true);
}

void
ServiceServer::send(Message *in, Message *out)
{
}

void
ServiceServer::sendAsync(Message *in)
{
    processOut(in);

    // transmit
}

void
ServiceServer::dispatch()
{
    bool hasResp;
    Message msgBuf;

    transport->accept();
    while (!stopRequested.load()) {
	transport->recv(&msgBuf);
	// XXX: Allow drops
	processIn(&msgBuf);
	hasResp = dispatch(&msgBuf);
	if (hasResp) {
	    // XXX: Allow drops
	    processOut(&msgBuf);
	    transport->send(&msgBuf);
	} 
    }
}

void
ServiceServer::processIn(Message *msg)
{
    for (auto it = filters.rbegin(); it != filters.rend(); it++) {
	(*it)->processIn(msg);
    }
}

void
ServiceServer::processOut(Message *msg)
{
    for (auto it = filters.begin(); it != filters.end(); it++) {
	(*it)->processOut(msg);
    }
}

}; };

