
namespace Celestis { namespace RPC {

class MessageFilter {
public:
    enum Status {
	OK, // Processed succesfully
	DROP, // Drop message
	AUTH_FAILED, // Authentication failed
	MALFORMED_MESSAGE, // Malformed message
    };
    virtual ~MessageFilter() = 0;
    virtual Status processIn(Message *msg) = 0;
    virtual Status processOut(Message *msg) = 0;
};

}; };

