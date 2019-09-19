
#include <string>
#include <iostream>

#include <thread>

#include <celestis/channel.h>
#include <celestis/inetaddress.h>
#include <celestis/inet4address.h>
#include <celestis/inetendpoint.h>
#include <celestis/datagramsocket.h>

using namespace std;
using namespace Celestis;

Channel<int> ch;

void
server()
{
    DatagramSocket sock;
    InetEndpoint endp;
    char *buf = new char[512];
    size_t len = 512;

    sock.bind(5000, Inet4Address::Any());

    ch.put(1);

    sock.recv(&endp, buf, &len);

    cout << "Recv from: " << endp.toString() << endl;
}

void
client()
{
    DatagramSocket sock;
    InetEndpoint endp = InetEndpoint(Inet4Address::Loopback(), 5000);
    char *buf = new char[512];
    size_t len = 512;

    sock.bind(0, Inet4Address::Any());

    ch.get();

    sock.send(endp, buf, len);
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

