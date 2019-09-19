#ifndef IOGEN_RANDOMIO_H
#define IOGEN_RANDOMIO_H

#include "ioinstr.h"

#include <string>
#include <random>
#include <vector>

// TODO : Document

enum class Dist {
	BERNOULLI,
	BINOMINAL,
	UNIFORM 
};

class RandomIO {
private:
    const Dist distribution;
    const std::string seed;
    const double rw_ratio;
    const std::vector<int> dist_params;

    void * initialize_engine();
    uint64_t get_random(void * engine, std::default_random_engine * seeder);
public:
    RandomIO(Dist distribution, const std::string &seed, double rw_ratio,
             const std::vector<int> &dist_params);

    std::vector<IOInstr> gen(int num);
};


#endif //IOGEN_RANDOMIO_H
