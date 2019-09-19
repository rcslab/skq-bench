#include "randomio.h"

#include <random>

RandomIO::RandomIO(const Dist distribution, const std::string &seed, const double rw_ratio,
                   const std::vector<int> &dist_params) : distribution(distribution),
                   seed(seed), rw_ratio(rw_ratio), dist_params(dist_params) {}

std::vector<IOInstr>
RandomIO::gen(int num)
{
    // Prepare seed
    std::seed_seq seed_seq (this->seed.begin(),this->seed.end());

    // Bernoulli distribution determines a sequence read/write operations that 
    // respect the ratio.
    std::default_random_engine bernoulli_engine(seed_seq);
    std::bernoulli_distribution rw_dist(this->rw_ratio);

    // Uniform distribution with upper and lower bounds to determine R/W size.
    std::default_random_engine main_engine(seed_seq);
    std::vector<IOInstr> instructions = std::vector<IOInstr>();

    // Initialize distribution
    void * dist = this->initialize_engine();

    for (int i = 0; i < num; i++) {
        auto opcode = static_cast<Opcode>(rw_dist(bernoulli_engine));
        size_t size = this->get_random(dist, &main_engine);

        IOInstr instr = IOInstr(size, opcode);
        instructions.emplace_back(instr);
    }

    return instructions;
}

void *
RandomIO::initialize_engine()
{
    void * dist = nullptr;

    switch (this->distribution) {
        case Dist::BERNOULLI : {
            dist = new std::bernoulli_distribution(this->dist_params.at(0));
            break;
        }
        case Dist::BINOMINAL : {
            dist = new std::binomial_distribution(this->dist_params.at(0), 
            		this->dist_params.at(1));
            break;
        }
        case Dist::UNIFORM : {
            dist = new std::uniform_int_distribution(this->dist_params.at(0), 
            		this->dist_params.at(1));
            break;
        }    
    }
    return dist;
}

uint64_t
RandomIO::get_random(void * engine, std::default_random_engine * seeder)
{
    std::default_random_engine dre = *seeder;
    switch (this->distribution) {
        case Dist::BERNOULLI: {
            auto res = static_cast<size_t>(((std::bernoulli_distribution *) engine)->
            		operator()(*seeder));
            return res;
        }
        case Dist::BINOMINAL: {
            auto * dist = static_cast<std::binomial_distribution<int> *>(engine);
            auto res = dist->operator()(dre);
            return static_cast<uint64_t>(res);
        }
        case Dist::UNIFORM: {
            auto * dist = static_cast<std::uniform_int_distribution<int> *>(engine);
            auto res = dist->operator()(dre);
            return static_cast<uint64_t>(res);
        }
    }
}


