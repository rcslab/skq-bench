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
#include <celestis/OSD/memdisk.h>

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
        {100 * MEGABYTES},
    };
    vector<Disk *> disks;
    size_t size = 0;

    Debug_OpenLog("ondisk_garbage_test.log");
    for(const auto &file: files) {
        Disk *temp = new MemDisk(file.size, 1);
        disks.push_back(temp);
        size += file.size;
    }

    DiskOSD * os = new DiskOSD(1 << 27);
    os->initialize(disks);
    os->log();
    auto bitmap = os->open(1); // NOLINT
    for (int i = 0; i < 1000; i++) {
        TEST_ASSERT(bitmap->stats().size > 0 );
        cout << i << endl;
        CVNode * n = os->create();
        n->truncate(2 *BLOCK_SIZE);
        auto arr = SGArray();
        arr.add(0, 2 * BLOCK_SIZE, malloc(2 * BLOCK_SIZE));
        n->read(arr);
        n->write(arr);
        n->close();
        TEST_ASSERT(bitmap->stats().size > 0 );
        TEST_ASSERT(bitmap->stats().asize < 30000 );
    }
    os->sync();
    os->log();

    for (int i = 2; i < 1002; i++) {
        os->remove(i);
    }
    os->log();
    os->sync();
    os->sync();
    os->sync();
    os->log();
    auto stats = os->stats(); // NOLINT
    TEST_ASSERT ( stats.fspace > 11300);
    PStats_Log();
    return 0;
}
