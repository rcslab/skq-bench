
#ifndef __PERFBENCH_H__
#define __PERFBENCH_H__

#include <cstdint>
#include <cmath>

#include <iostream>
#include <iomanip>

#include <celestis/stopwatch.h>
#include <celestis/debug.h>

namespace perfbench {

template <typename _T>
std::function<_T()>
autotime(std::function<_T(uint64_t)> F, uint64_t mintime_ms = 100)
{
    static const uint64_t MAX_N = 1024*1024*1024;
    for (uint64_t n = 1; n < MAX_N; n *= 2) {
	Celestis::Stopwatch sw;

	sw.start();
	F(n);
	sw.stop();

	if (sw.elapsedMS() > mintime_ms) {
	    return [F, n](){ return F(n); };
	}
    }

    PANIC();
}

template <typename _T>
std::pair<_T, double>
measure(std::function<_T(void)> F)
{
    static const int MIN_ITER = 5;
    static const int MAX_ITER = 25;
    _T values[MAX_ITER];
    _T mean, var;
    double sd;

    for (int i = 0; i < MAX_ITER; i++) {
	values[i] = F();

	mean = 0;
	var = 0;
	for (int j = 0; j <= i; j++) {
	    mean += values[j];
	}
	mean /= (i + 1);
	for (int j = 0; j <= i; j++) {
	    var += (values[j] - mean)*(values[j] - mean);
	}
	var /= (i + 1);
	sd = std::sqrt(var);

	if (i > MIN_ITER && (sd / mean < 0.05))
	    break;
    }

    return std::pair(mean, sd);
}

template <typename _T>
void
show(const std::string &name, const std::string &type, std::pair<_T,double> result)
{
    std::cout << name << ", "
	      << std::setprecision(2) << std::fixed << result.first << ", "
	      << std::setprecision(2) << std::fixed
	      << (result.second/result.first*100) << "%, "
	      << type << std::endl;
}

};

#endif /* __PERFBENCH_H__ */

