
#include <string>
#include <iostream>

#include <sys/types.h>
#include <sys/socket.h>

#include <unistd.h>
#include <fcntl.h>

#include <celestis/debug.h>
#include <celestis/eventloop.h>
#include <celestis/event.h>
#include <celestis/eventtimer.h>
#include <celestis/eventsock.h>
#include <celestis/eventaio.h>

using namespace std;
using namespace Celestis;

#define BUFSZ 512

void
IOCB(EventAIO *io)
{
    cout << "IO Completed!" << endl;
}

int
main(int argc, const char *argv[])
{
    //int status;
    //int fd;
    //char tmpfile[64] = "/tmp/libevent_test.XXXXXX";
    //char buf[512];
    //char buf2[512];
    //EventLoop el;

    Debug_OpenLog("event.log");

    cout << "AIO test" << endl;

#if 0
    status = mkstemp(&tmpfile[0]);
    if (status == -1) {
        perror("mkstemp");
        return 1;
    }

    fd = open(tmpfile, O_RDWR);
    if (fd < 0) {
	perror("open");
	return 1;
    }

    cout << "AIO read test" << endl;

    arc4random_buf(&buf, BUFSZ);

    EventAIO aio1 = EventAIO(&IOCB);
    el.addEvent(&aio1);
    aio1.write(fd, &buf, 0, BUFSZ);
    el.enterLoop();

    cout << "AIO write test" << endl;

    EventAIO aio2 = EventAIO(&IOCB);
    el.addEvent(&aio2);
    aio2.read(fd, &buf2, 0, BUFSZ);
    el.enterLoop();

    if (memcmp(&buf, &buf2, BUFSZ) != 0) {
	cout << "Read/Write Mismatch" << endl;
	return 1;
    }

    close(fd);
    unlink(tmpfile);
#endif

    cout << "Done" << endl;

    return 0;
}

