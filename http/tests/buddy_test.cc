#include <iostream>
#include <string>
#include <string.h>
#include <thread>
#include <unordered_set>

#include <sys/types.h>

#include <celestis/debug.h>
#include <celestis/pstats.h>
#include <celestis/buddyalloc.h>

#include "test.h"

using namespace std;
int
main(int argc, const char *argv[])
{
    size_t page = 12;
    size_t log2_size = 25;
    auto buddy = new BuddyAllocator(1 << log2_size, page, 12, 100);
    size_t n = (1 << (log2_size - page));
    TEST_ASSERT((n * (1 << page)) == (1 << log2_size));
    unordered_set<void *> buffers;
    for (int i = 0; i < n; i++) {
        auto temp = buddy->alloc(1 << page);
        TEST_ASSERT(temp != nullptr);
        TEST_ASSERT(buffers.find(temp) == buffers.end());
        buddy->debugVerify();
        buffers.insert(temp);
    }

    TEST_ASSERT(buddy->stats().fspace == 0);
    for(auto p : buffers) {
        buddy->free(p);
        buddy->debugVerify();
    }
    TEST_ASSERT(buddy->stats().fspace == (1 << log2_size));
    buffers.clear();
    for (int i = 0; i < n; i++) {
        auto temp = buddy->alloc(1 << page);
        TEST_ASSERT(temp != nullptr);
        TEST_ASSERT(buffers.find(temp) == buffers.end());
        buddy->debugVerify();
        buffers.insert(temp);
    }
    TEST_ASSERT(buddy->stats().fspace == 0);
    for(auto p : buffers) {
        buddy->free(p);
        buddy->debugVerify();
    }
    TEST_ASSERT(buddy->stats().fspace == (1 << log2_size));
    PStats_Log();
    return 0;
}
