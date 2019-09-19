
namespace Celestis { namespace RPC {

class TCPTransport : public ServiceTransport
{
public:
    TCPTransport();
    virtual ~TCPTransport();
    // Client
    virtual void connect(const std::string &addr);
    virtual void disconnect();
    // Server
    virtual void listen(const std::string &addr);
    virtual void shutdown();
    virtual void accept();
    virtual void close();
    // IO
    virtual Status send(Message *msg);
    virtual Status recv(Message *msg);
private:
    Status readExact(uint8_t *data, size_t len);
    int listenFd;
    int clientFd;
};

}; };

