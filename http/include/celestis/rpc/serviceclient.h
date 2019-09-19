
namespace Celestis { namespace RPC {

class ServiceClient {
public:
    void appendFilter(MessageFilter *filt);
    void connect(const std::string &proto, const std::string &address);
    void disconnect();
//protected:
    virtual void dispatchOOB(Message *in, Message *out);
    void send(Message *in, Message *out);
    void sendAsync(Message *in);
private:
    void dispatch();
    void processIn(Message *msg);
    void processOut(Message *msg);
    std::vector<MessageFilter*> filters;
    std::unique_ptr<ServiceTransport> transport;
};

}; };

