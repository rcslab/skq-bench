
namespace Celestis { namespace RPC {

class ServiceTransport
{
public:
    enum Protocol {
	TCP,
	UDS, /* Unix Domain Sockets */
	UDP,
	SCTP
    };
    enum Status {
	OK,
	DISCONNECT,
	ERROR
    };
    virtual ~ServiceTransport() { }
    // Client
    virtual void connect(const std::string &addr) = 0;
    virtual void disconnect() = 0;
    // Server
    virtual void listen(const std::string &addr) = 0;
    virtual void shutdown() = 0;
    virtual void accept() = 0;
    virtual void close() = 0;
    // IO
    virtual Status send(Message *msg) = 0;
    virtual Status recv(Message *msg) = 0;
};

}; };

