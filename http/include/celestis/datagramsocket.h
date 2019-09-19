
#ifndef __CELESTIS_DATAGRAMSOCKET_H__
#define __CELESTIS_DATAGRAMSOCKET_H__

namespace Celestis
{

class DatagramSocket
{
public:
    DatagramSocket();
    ~DatagramSocket();
    void close();
    void bind(const InetEndpoint &endp);
    void bind(uint16_t port, const InetAddress &addr);
    void send(const InetEndpoint &endp, const char *buf, size_t len);
    void recv(InetEndpoint *endp, char *buf, size_t *len);
private:
    int sock;
    bool open;
    bool bound;
    InetEndpoint laddr;
};

}

#endif /* __CELESTIS_DATAGRAMSOCKET_H__ */

