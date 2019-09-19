#include <cstdlib>
#include <ctime>

#include <celestis/debug.h>
#include <celestis/pstats.h>
#include <celestis/OSD/diskosd.h>
#include <celestis/OSD/filedisk.h>
#include <celestis/OSD/multibtree.h>

#include "test.h"

#define MEGABYTES (1024 * 1000)
#define NUM_INODES (10)
#define BLOCK_SIZE (1 << 12)

#define ENTRIES 5000
#define KEY_MAX 1000001
#define VAL_MAX 1000001

typedef struct test_files {
    size_t size;
} test_files;

int main()
{
    vector<test_files> files = {
        {50 * MEGABYTES},
        {50 * MEGABYTES}
    };
    vector<Disk *> disks;
    size_t size = 0;

    for(const auto &file: files) {
        Disk * temp = new FileDisk(file.size);
        disks.push_back(temp);
        size += file.size;
    }


    DiskOSD * os = new DiskOSD();
    Debug_OpenLog("btree.log");
    os->initialize(disks);

    auto n = os->create();
    std::srand(10); unordered_map<size_t, size_t> keys;
    auto multi = MultiBTree<size_t, size_t>(n, 1, 12);
    BlockBackedBTree<size_t, size_t> * t = multi[0];
    // Fill
    MSG("Prefilling");
    for (int i = 1; i < ENTRIES + 1; i++){
        size_t key = arc4random_uniform(KEY_MAX);
        size_t val = arc4random_uniform(VAL_MAX);
        t->insert(key, val);
        if (keys.find(key) == keys.end()) {
            keys[key] = 1;
        } else {
            keys[key] += 1;
        }
        auto g = t->get(key);
        TEST_ASSERT(g.found());
        auto ref = t->getClosest(0);
        while(ref.found()) {
            auto next = ref.next(); 
            if (next.found()) {
                TEST_ASSERT(ref.key() <= next.key());
                ref = next;
            } else {
                break;
            }
        }
        for (auto &&e : keys) {
            auto g = t->get(e.first);
            TEST_ASSERT(g.found());
        }
    }
    // Check AGAIN
     // PELT IT
    MSG("Random Insert/Removal");
    for (int i = 1; i < ENTRIES; i++){
        size_t action = arc4random_uniform(6);
        size_t key;
        if (action <= 5) {
            size_t random = arc4random_uniform(keys.size());
            auto e = next(begin(keys), random);
            auto ref = t->get(e->first);
            TEST_ASSERT(ref.found());
            t->remove(ref);
            e->second--;
            key = e->first;
            if (e->second == 0) {
                keys.erase(e->first);
            }
        } else {
            key = arc4random_uniform(KEY_MAX);
            size_t val = arc4random_uniform(VAL_MAX);
            MSG("INSERT - %lu", key);
            t->insert(key, val);
            if (keys.find(key) == keys.end()) {
                keys[key] = 1;
            } else {
                keys[key] += 1;
            }
 
        }

        auto ref = t->getClosest(0);
        while(ref.found()) {
            auto next = ref.next(); 
            if (next.found()) {
               TEST_ASSERT(ref.key() <= next.key());
            }
            ref = next;
        }

        for (auto &&e : keys) {
            auto g = t->get(e.first);
            TEST_ASSERT(g.found());
        }
    }
    PStats_Log();
}
