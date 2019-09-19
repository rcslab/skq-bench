#include <iostream>
#include <string>
#include <string.h>
#include <thread>
#include <vector>
#include <chrono>

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
#define BLOCK_SIZE (1 << 12)


typedef struct test_files {
    size_t size;
} test_files;

int
main(int argc, const char *argv[])
{
    vector<test_files> files = {
        {50 * MEGABYTES},
        {50 * MEGABYTES}
    };
    vector<Disk *> disks;
    size_t size = 0;

    for(const auto &file: files) {
        Disk *temp = new FileDisk(file.size);
        disks.push_back(temp);
        size += file.size;
    }

    Debug_OpenLog("vectorized.log");

    DiskOSD * os = new DiskOSD(1 << 27);
    os->initialize(disks);
    auto v = os->create();
    const int blocks = 10000;
    v->truncate(blocks * BLOCK_SIZE);
    SGArray read_in = SGArray();
    read_in.add(0, blocks * BLOCK_SIZE, malloc(blocks * BLOCK_SIZE));
    v->read(read_in);
    v->write(read_in);
    v->close();
    os->unmount();
    delete os;
    os = new DiskOSD(1 << 27);
    os->mount(disks);
    LOG("SUCCESSFULLY MOUNTED");
    LOG("ATTEMPTING OPENING OF 2");
    v = os->open(2);
    LOG("OPENED 2");
    LOG("INODE 2 - %s", v->to_string().c_str());
    os->log();
    TEST_ASSERT(blocks * BLOCK_SIZE == v->stats().size);
    TEST_ASSERT(blocks * BLOCK_SIZE == v->stats().asize);
    LOG("ATTEMPTING TO READ");
    read_in = SGArray();
    for(int i = 0; i < blocks; i++) {
        read_in.add(i * BLOCK_SIZE, BLOCK_SIZE, malloc(BLOCK_SIZE));
    }
    v->read(read_in);
    LOG("CAN READ");
    os->log();
    PStats_Log();
    return 0;
}

