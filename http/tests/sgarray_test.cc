#include <celestis/debug.h>
#include <celestis/threadpool.h>
#include <celestis/OSD/diskosd.h>
#include <celestis/OSD/filedisk.h>

#include "test.h"

#define MEGABYTES (1024 * 1000)
#define NUM_INODES (10)
#define BLOCK_SIZE (1 << 12)


typedef struct test_files {
    size_t size;
} test_files;

int
main() 
{
    vector<test_files> files = {
        {5 * MEGABYTES},
        {5 * MEGABYTES},
    };
    vector<Disk *> disks;
    size_t size = 0;

    for(const auto &file: files) {
        Disk * temp = new FileDisk(file.size);
        disks.push_back(temp);
        size += file.size;
    }

    DiskOSD * os = new DiskOSD(1 << 27);
    os->initialize(disks);

    auto n = os->create();
    n->truncate(15000);
    auto inode = n->stats().inum;

    auto arr = SGArray(); 
    arr.add(1000, 10000, malloc(10000));
    n->read(arr);
    char * c = (char *)arr.get(0)->buffer;
    c[0] = 'h';
    c[1] = 'e';
    c[2] = 'l';
    n->write(arr);

    arr = SGArray();
    arr.add(1001, 10000, malloc(10000));
    n->read(arr);
    c = (char *)arr.get(0)->buffer; // NOLINT
    TEST_ASSERT(c[0] == 'e');
    TEST_ASSERT(c[1] == 'l');


    // Write with buffer to new file
    arr = SGArray();
    size_t s = 10000;
    char * buff = (char *)malloc(s);
    arr.add(1000, 10000, buff);
    n->read(arr);
    TEST_ASSERT(buff[0] == 'h');
    TEST_ASSERT(buff[1] == 'e');
    TEST_ASSERT(buff[2] == 'l');
    buff[0] = 'T';
    buff[1] = 'O';
    buff[2] = 'L';
    buff[3] = 'D';
    n->write(arr);
    free(buff);

    os->unmount();
    os->mount(disks);

    n = os->open(inode);
    // READ FROM DISK WITH OWN BUFFER
    arr = SGArray();
    buff = (char *)malloc(s);
    arr.add(1000, 10000, buff);
    n->read(arr);
    TEST_ASSERT(buff[0] == 'T');
    TEST_ASSERT(buff[1] == 'O');
    TEST_ASSERT(buff[2] == 'L');
    TEST_ASSERT(buff[3] == 'D');
    free(buff);

    os->unmount();
    os->mount(disks);
    n = os->open(inode);
    // READ FROM DISK WITHOUT BUFFER
    arr = SGArray();
    arr.add(1002, 10000, malloc(10000));
    n->read(arr);
    buff = (char *)arr.get(0)->buffer; // NOLINT
    TEST_ASSERT(buff[0] == 'L');
    TEST_ASSERT(buff[1] == 'D');
    n->close();

    n = os->create();
    inode = n->stats().inum;
    n->truncate(100 * BLOCK_SIZE);
    arr = SGArray();
    arr.add(12 * BLOCK_SIZE, BLOCK_SIZE, malloc(BLOCK_SIZE));
    arr.add(99 * BLOCK_SIZE, BLOCK_SIZE, malloc(BLOCK_SIZE));
    n->read(arr);
    for (int i = 0; i < 2; i++ ) {
        buff = (char *)arr.get(i)->buffer;
        buff[0] = '4';
        buff[1] = '2';
        buff[2] = '0';
    }
    n->write(arr);
    
    os->unmount();
    os->mount(disks);
    n = os->open(inode);
    arr = SGArray();
    arr.add(12 * BLOCK_SIZE, BLOCK_SIZE, malloc(BLOCK_SIZE));
    arr.add(99 * BLOCK_SIZE, BLOCK_SIZE, malloc(BLOCK_SIZE));
    n->read(arr);

    for (int i = 0; i < 2; i++ ) {
        buff = (char *)arr.get(i)->buffer; // NOLINT
        TEST_ASSERT(buff[0] == '4');
        TEST_ASSERT(buff[1] == '2');
        TEST_ASSERT(buff[2] == '0');
    }
    return 0;
}

