

#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>

class Interface {
public:
    Interface();
    ~Interface();
    void refresh();
};

Interface::Interface() {
}

Interface::~Interface() {
}

void
Interface::refresh() {
    /*int status;
    struct ifaddrs *ifap;

    status = getifaddrs(&ifap);*/
}


