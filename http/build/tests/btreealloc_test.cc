#include <iostream>
#include <string>
#include <string.h>
#include <thread>
#include <vector>

#include <sys/types.h>

#include <celestis/debug.h>
#include <celestis/threadpool.h>
#include <celestis/OSD/dinode.h>
#include <celestis/OSD/blkptr.h>
#include <celestis/OSD/diskosd.h>
#include <celestis/OSD/btreealloc.h>
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
        {5 * MEGABYTES},
    };
    vector<Disk *> disks;
    size_t size = 0;

    Debug_OpenLog("ondisk_garbage_test.log");
    for(const auto &file: files) {
        Disk *temp = new FileDisk(file.size);
        disks.push_back(temp);
        size += file.size;
    }

    DiskOSD * os = new DiskOSD(1 << 27);
    os->initialize(disks);

    auto n = os->create();
    auto alloc = BTreeAlloc(disks, n, 12);
    MSG("%s", alloc.to_string().c_str());
    auto p = alloc.alloc(4096)[0];
    MSG("GIVEN %s", blkptr_to_str(p).c_str());
    MSG("%s", alloc.to_string().c_str());
    alloc.expire(p);
    MSG("%s", alloc.to_string().c_str());
    os->log();
    PStats_Log();
    return 0;
}
