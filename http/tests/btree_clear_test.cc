#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <string.h>
#include <sys/types.h>

#include <celestis/blockbackedbtree.h>
#include <celestis/testing/osdhelpers.h>
#include <celestis/OSD/cvnodealloc.h>

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
    Debug_OpenLog("vnode_create.log");
    auto os = initOSD(1 << 30, {100 * MEGABYTES, 100 * MEGABYTES});
    BlockAlloc bc = BlockAlloc();
    auto node = os->create();
    bc.init(node, 8);
    auto tree = new BlockBackedBTree<size_t, size_t>(&bc, bc.alloc());
    for (int i = 1; i < 100; i++) {
        tree->insert(i, i);
        for (int t = 1; t <= i; t++) {
            auto ref = tree->get(t);
            TEST_ASSERT(ref.found());
        }

    }
    tree->insert(1, 100);
    auto ref = tree->get(1);
    TEST_ASSERT(ref.val() == 1);
    ref = ref.next();
    TEST_ASSERT(ref.val() == 100);
    ref = ref.next();
    TEST_ASSERT(ref.key() == 2);
    TEST_ASSERT(ref.val() == 2);
    for (int i = 2; i < 100; i ++) {
        ref = tree->get(i);
        tree->remove(ref);
        for (int t = i + 1; t < 100; t++) {
            auto ref = tree->get(t);
            TEST_ASSERT(ref.found());
        }

    }
    ref = tree->get(1);
    while(ref.found())
    {
        ref = ref.next();
    }
    return 0;
}


