
#include <cassert>

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <exception>
#include <thread>

#include <celestis/rpc/message.h>
#include <celestis/rpc/messagefilter.h>
#include <celestis/rpc/servicetransport.h>
#include <celestis/rpc/serviceclient.h>
#include <celestis/rpc/serviceserver.h>
#include <celestis/rpc/serializationexception.h>

using namespace std;
using namespace Celestis::RPC;

class TestServer : public ServiceServer
{
public:
    TestServer() : ServiceServer() {
    }
    virtual ~TestServer() {
    }
    bool dispatch(Message *msg) {
	msg->unseal();
	assert(msg->readU8() == 150);
	cout << "Server OK" << endl;
	msg->seal();
	stop();
	return true;
    }
};

void
ClientThread(void)
{
    ServiceClient clt;
    Message out;
    Message in;
    out.appendU8(150);
    out.seal();

    try {
	clt.connect("TCP", "127.0.0.1");
	clt.send(&out, &in);
	clt.disconnect();
    } catch (exception &e) {
	cout << "Caught: " << e.what() << endl;
    }

    in.unseal();
    assert(in.readU8() == 150);

    cout << "Client OK" << endl;
}


int
main(int argc, const char *argv[])
{
    TestServer srv;
    thread client = thread(&ClientThread);

    try {
	srv.start("TCP", "0.0.0.0");
    } catch (exception &e) {
	cout << "Caught: " << e.what() << endl;
    }

    client.join();
}

