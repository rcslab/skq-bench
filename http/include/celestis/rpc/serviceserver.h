
#ifndef __CELESTIS_SERVICESERVER_H__
#define __CELESTIS_SERVICESERVER_H__

#include <atomic>

namespace Celestis { namespace RPC {

class ServiceServer {
protected:
    ServiceServer();
    virtual ~ServiceServer();
public:
    void appendFilter(MessageFilter *filt);
    void start(const std::string &proto, const std::string &address);
    void stop();
protected:
    virtual bool dispatch(Message *in) = 0;
    void send(Message *in, Message *out);
    void sendAsync(Message *in);
private:
    void dispatch();
    void processIn(Message *msg);
    void processOut(Message *msg);
    std::atomic_bool stopRequested;
    std::vector<MessageFilter*> filters;
    std::unique_ptr<ServiceTransport> transport;
//    std::vector<TransportThread*> ioThreads;
};

}; };

#endif /* __CELESTIS_SERVICESERVER_H__ */

