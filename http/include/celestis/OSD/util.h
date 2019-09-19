#ifndef __CELESTIS_FS_UTIL_H__
#define __CELESTIS_FS_UTIL_H__

#include <sys/time.h>
#include <sstream>

#include "defines.h"
static inline uint64_t
get_UTC()
{
    struct timeval tp;
    gettimeofday(&tp, 0);
    return tp.tv_sec;
}

static inline size_t
align_to_block(size_t num, size_t block_size = DEFAULT_BLOCK_SIZE) 
{
    int div = (num % block_size) > 0 ? 1 : 0;
    return ((num / block_size) + div) * block_size;
}

static inline size_t
block_units(size_t num, size_t block_size = DEFAULT_BLOCK_SIZE) 
{
    int div = (num % block_size) > 0 ? 1 : 0;
    return ((num / block_size) + div);
}



std::string tab_prepend(std::stringstream &s, int tab_spacing);

#endif
