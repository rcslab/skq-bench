
#include <string>
#include <iostream>
#include <exception>
#include <thread>

#include <celestis/debug.h>
#include <celestis/channel.h>
#include <celestis/inetaddress.h>
#include <celestis/inet4address.h>
#include <celestis/inetendpoint.h>
#include <celestis/stream.h>
#include <celestis/networkstream.h>
#include <celestis/socket.h>
#include <celestis/serversocket.h>

using namespace std;
using namespace Celestis;

Channel<int> ch;

void
server()
{
    ServerSocket sock;
    InetEndpoint endp;

    try {
	sock.setReuseAddress(true);
	sock.setReusePort(true);
	sock.bind(5000);
    } catch (const exception &e) {
	cout << "Caught: " << e.what() << endl;
	throw e;
    }

    ch.put(1);

    Socket s = sock.accept();
    s.close();

    cout << "Connected from: " << endp.toString() << endl;
}

void
client()
{
    Socket sock;
    InetEndpoint endp = InetEndpoint(Inet4Address::Loopback(), 5000);

    sock.bind(Inet4Address::Any(), (uint16_t)0);

    ch.get();

    sock.connect(endp);

    cout << "Connected" << endl;

    sock.close();
}

int
main(int argc, const char *argv[])
{
    thread srv;
    thread clt;

    Debug_OpenLog("test.log");

    cout << "Starting" << endl;

    srv = thread(&server);
    clt = thread(&client);

    srv.join();
    clt.join();

    cout << "Done" << endl;

    return 0;
}

