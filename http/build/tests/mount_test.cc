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
#define BLOCK_SIZE 4096


typedef struct test_files {
    size_t size;
} test_files;

void
short_mount()
{
    vector<test_files> files = {
        {5 * MEGABYTES},
        {5 * MEGABYTES},
    };
    vector<Disk *> disks;
    size_t size = 0;

    Debug_OpenLog("mount_test.log");
    for(const auto &file: files) {
        Disk *temp = new FileDisk(file.size);
        disks.push_back(temp);
        size += file.size;
    }

    DiskOSD * os = new DiskOSD(1 << 26);
    os->initialize(disks);
    CVNode * m = os->create();
    m->truncate(100);
    m->close();
    auto inode_num = m->stats().inum;
    os->unmount();
    os->mount(disks);
    m = os->open(inode_num); // NOLINT
    TEST_ASSERT(m->stats().size == 100);
    
}

int
long_mount()
{
    vector<test_files> files = {
        {5 * MEGABYTES},
        {20 * MEGABYTES},
    };
    vector<Disk *> disks;
    size_t size = 0;

    Debug_OpenLog("mount_test.log");
    for(const auto &file: files) {
        Disk *temp = new FileDisk(file.size);
        disks.push_back(temp);
        size += file.size;
    }

    DiskOSD * os = new DiskOSD(1 << 26);
    os->initialize(disks);

    CVNode * m = os->create();
    auto NUM = m->stats().inum;
    m->truncate(BLOCK_SIZE);
    auto entries = SGArray();
    entries.add(0, BLOCK_SIZE, malloc(BLOCK_SIZE));
    m->read(entries);
    for(auto &e : entries) {
        char * entry = (char *) e.buffer;
        entry[0] = 'h';
        entry[1] = 'e';
        entry[2] = 'l';
        entry[3] = 'l';
        entry[4] = 'o';
    }
    m->write(entries);

    LOG("BEFORE %s", os->to_string().c_str());
    os->unmount();
    os->mount(disks);
    LOG("AFTER %s", os->to_string().c_str());

    CVNode * n = os->create();
    size_t bs = (1 << DEFAULT_BLOCK_SIZE);
    n->truncate(10 * bs);
    entries = SGArray();
    entries.add(0, 10 * BLOCK_SIZE, malloc(10 * BLOCK_SIZE));
    n->read(entries);
    n->close();
    m = os->open(NUM);
    entries = SGArray();
    entries.add(0, BLOCK_SIZE, malloc(BLOCK_SIZE));
    m->read(entries);

    for(auto &e : entries) {
        char * entry = (char *) e.buffer;
        TEST_ASSERT(entry[0] == 'h');
        TEST_ASSERT(entry[1] == 'e');
        TEST_ASSERT(entry[2] == 'l');
        TEST_ASSERT(entry[3] == 'l');
        TEST_ASSERT(entry[4] == 'o');
        entry[0] = '6';
        entry[2] = '4';
        entry[4] = '0';
        LOG("TEST: %s", entry);
    }

    m->write(entries);
    entries = SGArray();
    entries.add(0, BLOCK_SIZE, malloc(BLOCK_SIZE));
    m->read(entries);
    for(auto &e : entries) {
        char * entry = (char *) e.buffer;
        TEST_ASSERT(entry[0] == '6');
        TEST_ASSERT(entry[1] == 'e');
        TEST_ASSERT(entry[2] == '4');
        TEST_ASSERT(entry[3] == 'l');
        TEST_ASSERT(entry[4] == '0');
        LOG("TEST: %s", entry);
    }
    os->unmount();
    delete os;
    os = new DiskOSD(1 << 26);
    os->mount(disks);
    LOG("NEW OSD - %s", os->to_string().c_str());
    m = os->open(NUM);
    entries = SGArray();
    entries.add(0, BLOCK_SIZE, malloc(BLOCK_SIZE));
    m->read(entries);
    LOG("TEST: %s", entries.get(0)->buffer);
    char * buff = (char *)entries.get(0)->buffer; // NOLINT
    TEST_ASSERT (buff[1] == 'e');
    TEST_ASSERT (buff[2] == '4');
    TEST_ASSERT (buff[3] == 'l');
    TEST_ASSERT (buff[4] == '0');
    LOG("%s", os->to_string().c_str());
    return 0;

}
int
main(int argc, const char *argv[]) 
{
    short_mount();
    return long_mount();
}
