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
#include <celestis/OSD/cosd.h>
#include <celestis/OSD/filedisk.h>

#include "test.h"

using namespace std; 

#define MEGABYTES (1024 * 1000)
#define NUM_INODES (10)
#define BLOCK_SIZE (1 << 12)


typedef struct test_files {
    size_t size;
} test_files;

STAT_TIMER(TEST, "test", PERF_UNITS_MSEC);

void 
reader_function(CObjectStore * os, int i) 
{
    CVNode * v = os->open(i);
    LOG("Process reader - getting inode - %s", v->to_string().c_str());
    if (v->stats().size == 0) {
        v->truncate(10 * BLOCK_SIZE);
        TEST_ASSERT( 10 * BLOCK_SIZE == v->stats().size);
    }
    for(int k = 1; k < 10; k++) {
        SGArray arr = SGArray();
        arr.add(k * BLOCK_SIZE, BLOCK_SIZE, malloc(BLOCK_SIZE));
        v->read(arr);
    }
    
    LOG("Process reader - DONE - %s", v->to_string().c_str());
    v->close();
}

void
writer_function(CObjectStore * os, int i, int t) 
{
    CVNode * v = os->open(i);
    LOG("Process writer - getting inode - %s", v->to_string().c_str());
    if (v->stats().size == 0){
        v->truncate(10 * BLOCK_SIZE);
        TEST_ASSERT( 10 * BLOCK_SIZE == v->stats().size);
    }

    SGArray arr = SGArray();
    arr.add(t * BLOCK_SIZE, BLOCK_SIZE, malloc(BLOCK_SIZE));

    v->read(arr);
    for(auto &e : arr) {
        char * entry = (char *)e.buffer;
        entry[0] = 'h';
        entry[1] = 'e';
        entry[2] = 'l';
        entry[3] = 'l';
        entry[4] = 'o';

    }
    v->write(arr);

    LOG("Process writer - DONE - %s", v->to_string().c_str());
    v->close();
}

void
transaction_manager(CObjectStore * os)
{
    for(int i = 1; i < 10; i++) {
        std::this_thread::sleep_for(0.5s);
        CVNode * n = os->open(1);
        // vector<rtuple> read_in = {
            // rtuple(0, BLOCK_SIZE)
        // };
        // auto e = n->read(read_in);
        n->close();
        os->sync();
        n = os->open(1);
        // e = n->read(0, 1);
        // vector<rtuple> read_in = {
            // rtuple(0, BLOCK_SIZE)
        // };
        // auto e = n->read(read_in);

        n->close();
    }
}

int
main(int argc, const char *argv[]) 
{
    vector<test_files> files = {
        {5 * MEGABYTES},
        {10 * MEGABYTES},
    };
    vector<Disk *> disks;
    size_t size = 0;

    Debug_OpenLog("filesys_test.log");
    for(const auto &file: files) {
        Disk * temp = new FileDisk(file.size);
        disks.push_back(temp);
        size += file.size;
    }

    DiskOSD * os = new DiskOSD(1 << 27);
    os->initialize(disks);
    for(int i = 2; i < 10; i++) {
        CVNode * n = os->create();
        TEST_ASSERT(n->_ref() == 2);
        n->close();
    }
    
    LOG("%s", os->to_string().c_str());

    STAT_TSAMPLE_START(TEST);
    std::thread thread0(transaction_manager, os);
    std::thread thread1(writer_function, os, 5, 3);
    std::thread thread2(reader_function, os, 5);
    std::thread thread3(reader_function, os, 5);
    std::thread thread4(reader_function, os, 7);
    std::thread thread5(writer_function, os, 7, 2);
    std::thread thread6(reader_function, os, 7);
    std::thread thread7(writer_function, os, 8, 5);
    std::thread thread8(reader_function, os, 8);
    std::thread thread9(writer_function, os, 9, 1);
    std::thread thread10(writer_function, os, 9, 2);
    std::thread thread11(reader_function, os, 9);
    
    thread0.join();
    thread1.join();
    thread2.join();
    thread3.join();
    thread4.join();
    thread5.join();
    thread6.join();
    thread7.join();
    thread8.join();
    thread9.join();
    thread10.join();
    thread11.join();
    STAT_TSAMPLE_STOP(TEST);

    LOG("%s", os->to_string().c_str());
    PStats_Log();
    return 0;
}
