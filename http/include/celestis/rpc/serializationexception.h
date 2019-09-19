
#include <string>
#include <exception>

namespace Celestis { namespace RPC {

class SerializationException : public std::exception
{
public:
    SerializationException(const std::string &cause) noexcept
	: cause(cause) { }
    virtual ~SerializationException() { }
    virtual const char* what() const noexcept { return cause.c_str(); }
private:
    std::string cause;
};

}; };

