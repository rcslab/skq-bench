
#ifndef __CELESTIS_HASH_H__
#define __CELESTIS_HASH_H__

#include <sstream>

#include <skein.h>

#define HASH_SIZE 32

struct Hash
{
    uint8_t hash[HASH_SIZE];
};

static inline
Hash
hash_data(const void *buf, size_t len)
{
    Skein_256_Ctxt_t ctx;
    Hash rval;

    Skein_256_Init(&ctx, 256);
    Skein_256_Update(&ctx, (const u08b_t *)buf, len);
    Skein_256_Final(&ctx, &rval.hash[0]);

    return rval;
}

static inline
std::string
hash_to_string(Hash h)
{
    std::stringstream str;

    for (uint8_t c : h.hash) {
	str << std::hex << (int)c;
    }

    return str.str();
}

static inline
bool
hash_is_equal(Hash a, Hash b)
{
    for(int i = 0; i < HASH_SIZE; i++) {
        if (a.hash[i] != b.hash[i] ) {

            return false;
        }
    }

    return true;
}

#endif

