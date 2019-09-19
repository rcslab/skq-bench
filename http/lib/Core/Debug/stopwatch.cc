
#include <sys/types.h>
#include <sys/sysctl.h>

#include <system_error>

#include <celestis/stopwatch.h>

namespace Celestis
{

Stopwatch::Stopwatch()
	: startCycles(0), stopCycles(0)
{
}

Stopwatch::~Stopwatch()
{
}

void
Stopwatch::start()
{
    startCycles = __builtin_readcyclecounter();
}

void
Stopwatch::stop()
{
    stopCycles = __builtin_readcyclecounter();
}

uint64_t
Stopwatch::elapsedCycles()
{
    return stopCycles - startCycles;
}

uint64_t
Stopwatch::elapsedMS()
{
#if defined(__FreeBSD__)
    const char *tsc = "machdep.tsc_freq";
#else
    const char *tsc = "machdep.tsc.frequency";
#endif
    uint64_t tscfreq;
    size_t len = 8;

    if (sysctlbyname(tsc, &tscfreq, &len, NULL, 0) < 0) {
	throw std::system_error(errno, std::system_category());
    }

    return 1000 * (stopCycles - startCycles) / tscfreq;
}

uint64_t
Stopwatch::elapsedUS()
{
#if defined(__FreeBSD__)
    const char *tsc = "machdep.tsc_freq";
#else
    const char *tsc = "machdep.tsc.frequency";
#endif
    uint64_t tscfreq;
    size_t len = 8;

    if (sysctlbyname(tsc, &tscfreq, &len, NULL, 0) < 0) {
	throw std::system_error(errno, std::system_category());
    }

    return 1000 * 1000 * (stopCycles - startCycles) / tscfreq;
}

}

