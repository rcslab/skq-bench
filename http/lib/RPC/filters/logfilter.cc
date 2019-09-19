
namespace Celestis { namespace RPC {

class LogFilter : public MessageFilter {
public:
    LogFilter()
    {
    }
    virtual ~LogFilter()
    {
    }
    virtual Status
    processIn(Message *msg)
    {
	sstream str;

	str << "IN" << endl;
	str << msg->dump() << endl;
    }
    virtual Status
    processOut(Message *msg)
    {
	sstream str;

	str << "OUT" << endl;
	str << msg->dump() << endl;
    }
};

}; };

