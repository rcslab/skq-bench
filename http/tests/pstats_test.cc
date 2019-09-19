
#include <iostream>
#include <string>
#include <unordered_map>

#include <celestis/debug.h>
#include <celestis/pstats.h>

using namespace std;

/*
 * Counter objects:
 * Keep a count of the number of events
 */
STAT_COUNTER(DISKIO, "disk.io", PERF_UNITS_NONE);

/*
 * Timer objects:
 * Track total time, average time, min/max, and std dev
 */
STAT_TIMER(CPUTIME, "cputime", PERF_UNITS_CYCLES);

/*
 * Guage objects:
 * Record a time series
 */
STAT_GUAGE(DISKLAT, "disk.latency", PERF_UNITS_MSEC);

int
main(int argc, const char *argv[])
{
    Debug_OpenLog("pstats_test.log");

    STAT_CSAMPLE(DISKIO, 1);
    STAT_CSAMPLE(DISKIO, 2);
    STAT_CSAMPLE(DISKIO, 3);

    TimerSample samp(&STAT_NAME(CPUTIME));
    samp.start();
    samp.stop();

    STAT_GSAMPLE(DISKLAT, 5);
    STAT_GSAMPLE(DISKLAT, 6);
    STAT_GSAMPLE(DISKLAT, 5);
    STAT_GSAMPLE(DISKLAT, 6);

    PStats_Log();
}

