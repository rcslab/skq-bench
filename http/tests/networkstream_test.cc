
#include <string>
#include <iostream>

#include <thread>

#include <celestis/debug.h>
#include <celestis/channel.h>
#include <celestis/inetaddress.h>
#include <celestis/inet4address.h>
#include <celestis/inetendpoint.h>
#include <celestis/stream.h>
#include <celestis/bufferedstream.h>
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
	sock.bind(5000);
	sock.setReuseAddress(true);
	sock.setReusePort(true);
    } catch (const exception &e) {
	cout << "Caught: " << e.what() << endl;
	throw e;
    }


    ch.put(1);

    Socket s = sock.accept();

    NetworkStream &str = s.getStream();
    char buf[4];

    str.read(&buf[0], 0, 4);
    ASSERT(strncmp(&buf[0], "\x55\x55\x55\x55", 4) == 0);
    str.write("\xAA\xAA\xAA\xAA", 0, 4);

    s.close();
    cout << "Recv from: "; // endp.toString() << endl;
}

void
client()
{
    Socket sock;
    InetEndpoint endp = InetEndpoint(Inet4Address::Loopback(), 5000);

    sock.bind(Inet4Address::Any(), (uint16_t)0);

    ch.get();

    sock.connect(endp);

    NetworkStream &str = sock.getStream();
    char buf[4];
    str.write("\x55\x55\x55\x55", 0, 4);
    str.read(&buf[0], 0, 4);
    ASSERT(strncmp(&buf[0], "\xAA\xAA\xAA\xAA", 4) == 0);

    sock.close();
}

int
main(int argc, const char *argv[])
{
    thread srv;
    thread clt;

    cout << "Starting" << endl;

    srv = thread(&server);
    clt = thread(&client);

    srv.join();
    clt.join();

    cout << "Done" << endl;

    return 0;
}

