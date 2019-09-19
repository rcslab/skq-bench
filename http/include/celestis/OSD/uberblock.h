#ifndef _UBERBLOCK_H_
#define _UBERBLOCK_H_

#include <celestis/hash.h>

#include "blkptr.h"

#define UB_MAGIC_NUM 0xdeadbea1
#define UB_VERSION 0x1

typedef struct uberblock {
    uint64_t magic;
    uint64_t txg;
    uint64_t timestamp;
    Hash checksum;    
    blkptr rootbp;
} uberblock;

uberblock uberblock_create_null();
uberblock uberblock_create_start(int disk_id);
bool uberblock_verify(uberblock ub, Hash h);
std::string uberblock_to_str(const uberblock &ub, int tab_spacing = 0);

#endif
