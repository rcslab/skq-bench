#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>

#include <celestis/debug.h>
#include <celestis/pstats.h>
#include <celestis/command.h>

using namespace std;

STAT_TIMER(ALLOC, "alloc.alloc", PERF_UNITS_CYCLES);
STAT_TIMER(FREE, "alloc.free", PERF_UNITS_CYCLES);
STAT_TIMER(BTREEI, "btree.insert", PERF_UNITS_CYCLES);
STAT_TIMER(BTREELEAF, "btree.insert.leaf", PERF_UNITS_CYCLES);
STAT_TIMER(BTREEBUCKET, "btree.insert.leaf.bucket", PERF_UNITS_CYCLES);
STAT_TIMER(BTREESINGLETON, "btree.insert.leaf.singleton", PERF_UNITS_CYCLES);
STAT_TIMER(BTREED, "btree.delete", PERF_UNITS_CYCLES);
STAT_TIMER(BTREEG, "btree.get", PERF_UNITS_CYCLES);

extern void Registry_Init();
extern std::map<std::string, PerfSource*> *stats;
std::unordered_map<PerfUnits, std::string> PerfUnitString = {
    {PERF_UNITS_NONE, ""},
    {PERF_UNITS_BYTES, "B"},
    {PERF_UNITS_KBYTES, "KiB"},
    {PERF_UNITS_MBYTES, "MiB"},
    {PERF_UNITS_GBYTES, "GiB"},
    {PERF_UNITS_SEC, "s"},
    {PERF_UNITS_MSEC, "ms"},
    {PERF_UNITS_USEC, "us"},
    {PERF_UNITS_NSEC, "ns"},
    {PERF_UNITS_CYCLES, "cycles"},
};


PerfSource::PerfSource(const std::string &name)
{
    LOG("Registering pstat: %s", name.c_str());
    Registry_Init();
    (*stats)[name] = this;
}

PerfCounter::PerfCounter(const std::string &name, PerfUnits units)
    : PerfSource(name), value(0), count(0), units(units)
{
}

PerfCounter::~PerfCounter()
{
}

void
PerfCounter::addSample(int64_t cnt)
{
    value += cnt;
    count += 1;
}

std::string
PerfCounter::getSummary()
{
    std::stringstream ss;

    ss << "total: " << value << PerfUnitString[units] << " count:" << count;

    return ss.str();
}

PerfTimer::PerfTimer(const std::string &name, PerfUnits units)
    : PerfSource(name), units(units), min(numeric_limits<uint64_t>::max()),
      max(0), total(0), count(0)
{
}

PerfTimer::~PerfTimer()
{
}

void
PerfTimer::addSample(uint64_t tm)
{
    if (tm < min)
        min = tm;
    if (tm > max)
        max = tm;
    total += tm;
    count++;
}

std::string
PerfTimer::getSummary()
{
    std::stringstream ss;

    if (count == 0) {
        ss << "no samples";
    } else {
        ss << "mean:" << total/count << PerfUnitString[units];
        ss << " count:" << count;
        ss << " min:" << min << PerfUnitString[units];
        ss << " max:" << max << PerfUnitString[units];
    }

    return ss.str();
}

TimerSample::TimerSample(PerfTimer *timer)
    : timer(timer), startTS(0), units(timer->units)
{
}

TimerSample::~TimerSample()
{
    ASSERT(startTS == 0);
}

void
TimerSample::start()
{
    ASSERT(startTS == 0);
    switch (units) {
        case PERF_UNITS_CYCLES:
        {
            startTS = __builtin_readcyclecounter();
            break;
        }
        case PERF_UNITS_MSEC:
        {
            startTS_c  = chrono::high_resolution_clock::now();
            break;
        }
        default:
        {
            PANIC();
        }
    }
}

void
TimerSample::stop()
{
    uint64_t time_passed = 0;
    switch (units) {
        case PERF_UNITS_CYCLES: 
        {
            ASSERT(startTS != 0);
            uint64_t stop = __builtin_readcyclecounter();
            time_passed = stop - startTS;
            break;
        }
        case PERF_UNITS_MSEC:
        {
            auto stop_c  = chrono::high_resolution_clock::now();
            time_passed = chrono::duration_cast<chrono::microseconds>(stop_c - startTS_c).count();
            break;
        }
        default:
        {
            PANIC();
        }
    }

    startTS = 0;
    timer->addSample(time_passed);
}

PerfGuage::PerfGuage(const std::string &name, PerfUnits units)
    : PerfSource(name), min(numeric_limits<uint64_t>::max()), max(0),
      total(0), count(0), units(units)
{
}

PerfGuage::~PerfGuage()
{
}

void
PerfGuage::addSample(uint64_t s)
{
    if (s < min)
        min = s;
    if (s > max)
        max = s;
    total += s;
    count++;
}

std::string
PerfGuage::getSummary()
{
    std::stringstream ss;

    if (count == 0) {
        ss << "";
    } else {
        ss << "mean:" << total/count << PerfUnitString[units];
        ss << " count:" << count;
        ss << " min:" << min << PerfUnitString[units];
        ss << " max:" << max << PerfUnitString[units];
    }
    return ss.str();
}

void
PStats_Log()
{
    for (auto &s : *stats) {
        Debug_Log(LEVEL_MSG, "%s : %s\n", s.first.c_str(), s.second->getSummary().c_str());
    }
}

void
cmd_pstats(IShell *shell, std::vector<std::string> args)
{
    shell->writeLine("Performance Statistics:");
    for (auto &s : *stats) {
        shell->writeLine(s.first + " : " + s.second->getSummary());
    }
}
DECLCMD(pstats, "Performance statistics", "");

