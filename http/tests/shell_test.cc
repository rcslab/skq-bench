
#include <unistd.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>

#include <celestis/debug.h>
#include <celestis/pstats.h>
#include <celestis/command.h>
#include <celestis/terminal.h>
#include <celestis/termui.h>
#include <celestis/shell.h>
#include <celestis/configuration.h>

using namespace std;

CVString testString = CVString("test.string", "Default String");
CVInteger testInteger = CVInteger("test.integer", 1);
CVFloat testFloat = CVFloat("test.float", 1.0);

STAT_COUNTER(DISKIO, "disk.io", PERF_UNITS_NONE);
STAT_GUAGE(DISKLAT, "disk.latency", PERF_UNITS_MSEC);
STAT_TIMER(CPUTIME, "cputime", PERF_UNITS_CYCLES);

int
main(int argc, const char *argv[])
{
    string str;
    TermUI t = TermUI(0);
    Shell s = Shell(t);

    t.init();
    s.setTitle("Test Shell");
    s.setStatus("Status Bar");
    s.process();
}

