#ifndef PSTATS_H_
#define PSTATS_H_

#include <unordered_map>
#include <chrono>

enum PerfUnits {
    PERF_UNITS_NONE,
    PERF_UNITS_BYTES,
    PERF_UNITS_KBYTES,
    PERF_UNITS_MBYTES,
    PERF_UNITS_GBYTES,
    PERF_UNITS_SEC,
    PERF_UNITS_MSEC,
    PERF_UNITS_USEC,
    PERF_UNITS_NSEC,
    PERF_UNITS_CYCLES,
};

class PerfSource
{
public:
    virtual std::string getSummary() = 0;
protected:
    PerfSource(const std::string &name);
};

class PerfCounter : public PerfSource
{
public:
    PerfCounter(const std::string &name, PerfUnits units);
    ~PerfCounter();
    void addSample(int64_t cnt);
    virtual std::string getSummary();
private:
    std::atomic<int64_t> value;
    std::atomic<int64_t> count;
    PerfUnits units;
};


class PerfTimer : public PerfSource
{
public:
    PerfTimer(const std::string &name, PerfUnits units);
    ~PerfTimer();
    virtual std::string getSummary();
protected:
    void addSample(uint64_t tm);
    PerfUnits units;
    friend class TimerSample;
private:
    std::atomic<uint64_t> min;
    std::atomic<uint64_t> max;
    std::atomic<uint64_t> total;
    std::atomic<uint64_t> count;
};

class TimerSample
{
public:
    TimerSample(PerfTimer *timer);
    ~TimerSample();
    void start();
    void stop();
private:
    PerfTimer *timer;
    uint64_t startTS;
    std::chrono::time_point<std::chrono::high_resolution_clock> startTS_c;        
    PerfUnits units;
};

class PerfGuage : public PerfSource
{
public:
    PerfGuage(const std::string &name, PerfUnits units);
    ~PerfGuage();
    void addSample(uint64_t tm);
    virtual std::string getSummary();
private:
    std::atomic<uint64_t> min;
    std::atomic<uint64_t> max;
    std::atomic<uint64_t> total;
    std::atomic<uint64_t> count;
    PerfUnits units;
};

#define STAT_NAME(_name)    STAT_##_name

#define STAT_COUNTER(_name, _path, _units) \
    PerfCounter STAT_##_name(_path, _units);

#define STAT_CSAMPLE(_name, _value) \
    STAT_##_name.addSample(_value);

#define STAT_TIMER(_name, _path, _units) \
    PerfTimer STAT_##_name(_path, _units);

#define STAT_TSAMPLE_START(_name) \
    TimerSample STAT_SAMPLE_##_name(&STAT_NAME(_name)); \
    STAT_SAMPLE_##_name.start();

#define STAT_TSAMPLE_STOP(_name) \
    STAT_SAMPLE_##_name.stop();

#define STAT_GUAGE(_name, _path, _units) \
    PerfGuage STAT_##_name(_path, _units);

#define STAT_GSAMPLE(_name, _value) \
    STAT_##_name.addSample(_value);

void PStats_Log();

#endif

