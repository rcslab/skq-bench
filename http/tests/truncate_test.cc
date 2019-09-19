#include <iostream>
#include <string>
#include <string.h>
#include <thread>
#include <vector>

#include <sys/types.h>

#include <celestis/debug.h>
#include <celestis/threadpool.h>
#include <celestis/OSD/dinode.h>
#include <celestis/OSD/diskosd.h>
#include <celestis/OSD/filedisk.h>

#include "test.h"

using namespace std; 

#define MEGABYTES (1024 * 1000)
#define NUM_INODES (10)
#define BLOCK_SIZE (4096)


typedef struct test_files {
    size_t size;
} test_files;


int
main(int argc, const char *argv[]) 
{

    vector<test_files> files = {
        {5 * MEGABYTES},
        {20 * MEGABYTES},
    };
    vector<Disk *> disks;
    size_t size = 0;

    Debug_OpenLog("truncate_test.log");
    for(const auto &file: files) {
        Disk *temp = new FileDisk(file.size);
        disks.push_back(temp);
        size += file.size;
    }

    DiskOSD * os = new DiskOSD(1 << 27);
    os->initialize(disks);

    CVNode * m = os->create();
    m->truncate(1);
    os->sync();
    TEST_ASSERT(m->stats().asize == 0);
    TEST_ASSERT(m->stats().size == 1);
    SGArray arr = SGArray();
    arr.add(0, 1, malloc(1));
    m->read(arr);
    TEST_ASSERT(m->stats().asize == 0);
    TEST_ASSERT(m->stats().size == 1);


    CVNode * n = os->create();
    n->truncate(10000);
    SGArray arr2 = SGArray();
    arr2.add(200, 5000, malloc(5000));
    n->read(arr2);
    os->sync();
    TEST_ASSERT(n->stats().asize == 0);
    TEST_ASSERT(n->stats().size == 10000);
    m->read(arr);
    m->write(arr);
    os->sync();
    TEST_ASSERT(m->stats().asize == 1);
    TEST_ASSERT(m->stats().size == 1);

    m->truncate(0);

    TEST_ASSERT(m->stats().asize == 1);
    TEST_ASSERT(m->stats().size == 0);
    os->sync();
    TEST_ASSERT(m->stats().asize == 0);
    TEST_ASSERT(m->stats().size == 0);
    os->sync();
    os->sync();
    TEST_ASSERT(m->stats().asize == 0);
    TEST_ASSERT(m->stats().size == 0);

}
