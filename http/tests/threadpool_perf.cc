
#include <string>
#include <sstream>
#include <iostream>

#include <unistd.h>

#include <celestis/stopwatch.h>
#include <celestis/threadpool.h>

#include "perfbench.h"

using namespace std;
using namespace Celestis;

#define TEST_FUNCS	200000

void
test(void)
{
}

double
enqueue_bench(uint64_t n)
{
    Stopwatch sw;
    ThreadPool tpool;

    sw.start();
    for (int i = 0; i < n; i++) {
	tpool.enqueue(&test);
    }
    tpool.drain_and_wait();
    sw.stop();

    if (sw.elapsedMS() == 0)
	return nan("");

    return n * 1000 / (sw.elapsedMS());
}

int
main(int argc, const char *argv[])
{
    std::stringstream name;
    ThreadPool tpool;
    size_t threads = tpool.concurrency();

    auto f = perfbench::autotime<double>(enqueue_bench, 200);
    auto r = perfbench::measure<double>(f);

    name << "threadpool " << threads << " threads";
    perfbench::show(name.str(), "Funcs/sec", r);
}

