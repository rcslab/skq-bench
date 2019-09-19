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
        {10 * MEGABYTES},
        {10 * MEGABYTES}
    };
    vector<Disk *> disks;
    size_t size = 0; 
    for(int i = 0 ; i < files.size(); i++) {
        auto file = files[i];
        Disk *temp = new MemDisk(file.size, i);
        disks.push_back(temp);
        size += file.size;
    }

    Debug_OpenLog("create_vnode_garbage.log");
    DiskOSD * os = new DiskOSD(1 << 26);
    os->initialize(disks);

    os->log();
    CVNode * bitmap = os->open(1);

    auto read_in = SGArray();
    read_in.add(0, bitmap->getSize(), malloc(bitmap->getSize()));
    bitmap->read(read_in);
    bitmap->read(read_in);

    CVNode * meta = os->open(0); // NOLINT
    TEST_ASSERT(meta->stats().size == 2 * 4096);
    for (int i = 2; i < 100; i++) {
        CVNode * n = os->create();
        TEST_ASSERT(meta->stats().size == (i + 1) * 4096);
        TEST_ASSERT(n->stats().inum == i);
        n->truncate(2 * BLOCK_SIZE);
        auto next = SGArray();
        next.add(0, 2 * BLOCK_SIZE, malloc(2 * BLOCK_SIZE));

        n->read(next);
        for(auto &entry : read_in) {
            char * char_entry = (char *) entry.buffer;
            char_entry[0] = 'h';
        }
        n->write(next);
        n->close();
        next.free();
    }

    bitmap->read(read_in);

    os->log();
    os->sync();
    os->log();

    for (int i = 2; i < 50; i++) {
        os->remove(i);
    }
    os->log();


    os->sync();
    os->sync();
    auto stat = os->stats(); // NOLINT
    os->sync();
    os->log();
    auto statafter = os->stats(); // NOLINT
    TEST_ASSERT(stat.fspace < statafter.fspace); 
    return 0;
}
