
#include <iostream>
#include <string>

#include <unistd.h>

#include <celestis/threadpool.h>

using namespace std;
using namespace Celestis;

void
test1(void)
{
    cout << "test1" << endl;
}

void
test2(int a)
{
    cout << "test2 " << a << endl;
}

int
test3(void)
{
    cout << "test3" << endl;
    return 5;
}

int
main(int argc, const char *argv[])
{
    ThreadPool tpool;

    cout << "Concurrency: " << tpool.concurrency() << endl;

    future<void> t1 = tpool.enqueue(&test1);
    tpool.enqueue(&test2, 1);
    future<int> t3 = tpool.enqueue(&test3);

    cout << "Done" << endl;

    t1.wait();
    cout << "test3 value: " << t3.get() << endl;
}

