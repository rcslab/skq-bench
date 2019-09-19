#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <string.h>
#include <sys/types.h>

#include <celestis/testing/osdhelpers.h>

#include "test.h"

using namespace std; 

#define MEGABYTES (1024 * 1000)
#define NUM_INODES (10)
#define BLOCK_SIZE (1 << 12)


#define FILES (1000)
#define SIZE (100)

int
main(int argc, const char *argv[]) 
{
    Debug_OpenLog("vnode_shrink.log");
    auto os = initOSD(1 << 30, {100 * MEGABYTES, 100 * MEGABYTES});
    createFiles(os, 1);
    auto n = os->open(2);
    n->truncate(BLOCK_SIZE * 10);
    SGArray arr = SGArray();
    for (int i = 0; i < 10; i++) {
        auto buff = (char *)malloc(BLOCK_SIZE);
        memset(buff, 0, BLOCK_SIZE);
        memset(buff, 'a', 100);
        arr.add(i * BLOCK_SIZE, BLOCK_SIZE, (void *)buff);
    }
    n->write(arr);
    n->truncate(100);
    n->close();
    os->sync();
    n = os->open(2); //NOLINT
    TEST_ASSERT(n->stats().asize == 100);
}

