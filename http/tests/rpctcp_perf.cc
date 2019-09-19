
#include <cassert>

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <exception>
#include <thread>

#include <celestis/stopwatch.h>
#include <celestis/rpc/message.h>
#include <celestis/rpc/messagefilter.h>
#include <celestis/rpc/servicetransport.h>
#include <celestis/rpc/serviceclient.h>
#include <celestis/rpc/serviceserver.h>
#include <celestis/rpc/serializationexception.h>

#include "perfbench.h"

using namespace std;
using namespace Celestis;
using namespace Celestis::RPC;

#define TEST_MESSAGES		100000

class TestServer : public ServiceServer
{
public:
    TestServer() : ServiceServer(), msgs(0) {
    }
    virtual ~TestServer() {
    }
    bool dispatch(Message *msg) {
	msg->unseal();
	switch (msg->readU8()) {
	    case 100:
		return false;
	    case 150:
		msg->seal();
		return true;
	    case 200:
		msg->seal();
		stop();
		return true;
	}
	return true;
    }
    int msgs;
};

double
AsyncClient(ServiceClient &clt, uint64_t n)
{
    Message out;
    Stopwatch sw;

    out.appendU8(100);
    out.seal();

    try {
	sw.start();
	for (int i = 0; i < n; i++) {
	    clt.sendAsync(&out);
	}
	{
	    Message out2;
	    Message in;
	    out2.appendU8(150);
	    out2.seal();
	    clt.send(&out2, &in);
	}
	sw.stop();
    } catch (exception &e) {
	cout << "Caught: " << e.what() << endl;
    }

    if (sw.elapsedMS() == 0)
	return nan("");

    return n * 1000 / sw.elapsedMS();
}

double
SyncClient(ServiceClient &clt, uint64_t n)
{
    Message out;
    Message in;
    Stopwatch sw;

    out.appendU8(150);
    out.seal();

    try {
	sw.start();
	for (int i = 0; i < n; i++) {
	    clt.send(&out, &in);
	}
	sw.stop();
    } catch (exception &e) {
	cout << "Caught: " << e.what() << endl;
    }

    if (sw.elapsedMS() == 0)
	return nan("");

    return n * 1000 / sw.elapsedMS();
}

void
ClientThread(void)
{
    ServiceClient clt;

    try {
	clt.connect("TCP", "127.0.0.1");
    } catch (exception &e) {
	cout << "Caught: " << e.what() << endl;
    }

    try {
	Message in, out;
	out.appendU8(150);
	out.seal();
	clt.send(&out, &in);
    } catch (exception &e) {
	cout << "Caught: " << e.what() << endl;
    }

    auto f = perfbench::autotime<double>([&clt](uint64_t n){ return AsyncClient(clt, n); }, 200);
    auto r = perfbench::measure<double>(f);
    perfbench::show("rpctcp async", "Msgs/sec", r);

    f = perfbench::autotime<double>([&clt](uint64_t n){ return SyncClient(clt, n); });
    r = perfbench::measure<double>(f);
    perfbench::show("rpctcp sync", "Msgs/sec", r);

    try {
	Message out;
	out.appendU8(200);
	out.seal();
	clt.sendAsync(&out);
	clt.disconnect();
    } catch (exception &e) {
	cout << "Caught: " << e.what() << endl;
    }
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

