
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

using namespace std;
using namespace Celestis;

int cnt = 0;

void
TimerCB(EventTimer &t)
{
    cout << "Timer fired!" << endl;
    cnt++;
    if (cnt == 10) {
	t.cancel();
    }
}

int
main(int argc, const char *argv[])
{
    EventLoop el;

    Debug_OpenLog("event_timer.log");

    cout << "Periodic Timer test" << endl;

    EventTimer t1 = EventTimer(el, &TimerCB);
    t1.periodic(100);
    el.enterLoop();

    cout << "Oneshot Timer test" << endl;

    EventTimer t2 = EventTimer(el, &TimerCB);
    t2.oneshot(100);
    el.enterLoop();

    cout << "Done" << endl;

    return 0;
}

