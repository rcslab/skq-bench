
#ifndef __CELESTIS_STOPWATCH_H__
#define __CELESTIS_STOPWATCH_H__

namespace Celestis
{

class Stopwatch
{
public:
    Stopwatch();
    ~Stopwatch();
    void start();
    void stop();
    uint64_t elapsedCycles();
    uint64_t elapsedMS();
    uint64_t elapsedUS();
private:
    uint64_t startCycles;
    uint64_t stopCycles;
};

}

#endif /* __CELESTIS_STOPWATCH_H__ */

