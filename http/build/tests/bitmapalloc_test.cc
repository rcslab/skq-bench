#include <iostream>
#include <string>

#include <celestis/debug.h>
#include <celestis/OSD/filedisk.h>
#include <celestis/OSD/bitmapalloc.h>

#include "test.h"

using namespace std;

#define MEGABYTES 1024 * 1000
#define NUM_INODES 10
#define BLOCK_SIZE 4096

typedef struct test_files {
    size_t size;
} test_files;

int
main(int argc, const char *argv[]) {
   
    const int NUM_OF_FILES = 2;
    test_files files[NUM_OF_FILES] = {
        {10 * MEGABYTES},
        {10 * MEGABYTES},
    };
    vector<Disk *> disks;
    Debug_OpenLog("bitmapalloc_test.log");
    for(int i = 0; i < NUM_OF_FILES; i ++) {
        test_files f = files[i];
        Disk * temp = new FileDisk(f.size);
        disks.push_back(temp);
    }
    vector<BEntry *> e = {
        new BEntry{
            .reference_count = 0,
            .size = (size_t)BLOCK_SIZE,
            .type = BlockType::NONE,
            .buffer = new char[BLOCK_SIZE],
            .dirty = false
        },
        new BEntry{
            .reference_count = 0,
            .size = (size_t)BLOCK_SIZE,
            .type = BlockType::NONE,
            .buffer = new char[BLOCK_SIZE],
            .dirty = false
        },

    };
    memset(e[0]->buffer, '0', BLOCK_SIZE);
    memset(e[1]->buffer, '0', BLOCK_SIZE);
    BEntry * one = e[0];
        
    size_t full_space = 1372 * 2;
    Allocator * a = new BitMapAllocator(disks, e, 12);
    TEST_ASSERT(a->stats().fspace == full_space);
    blkptr p = a->alloc(1)[0];
    TEST_ASSERT(one->buffer[0] == '1');
    a->free(p);
    TEST_ASSERT(one->buffer[0] == 'X');
    a->expire(p);
    TEST_ASSERT(one->buffer[0] == '0');
    p = a->alloc(1)[0];
    TEST_ASSERT(one->buffer[0] == '1');
    TEST_ASSERT(a->stats().fspace == full_space - 1);
    p = a->alloc(4096)[0];
    TEST_ASSERT(one->buffer[0] == '1');
    TEST_ASSERT(a->stats().fspace == full_space - 2);
    p = a->alloc((1300 * BLOCK_SIZE) + 1)[0];
    TEST_ASSERT(a->stats().fspace == full_space - 1303);
}
