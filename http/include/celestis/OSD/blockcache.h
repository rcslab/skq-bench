/**
 * @ingroup ObjectStoreDevice
 *
 * @class BlockCache
 *
 * @brief Blockcache is a simple cache used to hold onto virtual memory blocks. 
 *
 * @author Kenneth R Hancock
 */
#ifndef __CELESTIS_FS_BUFFERCACHE_H__
#define __CELESTIS_FS_BUFFERCACHE_H__

#include <list>

#include "../buddyalloc.h"
#include "defines.h"
#include "cvnode.h"

using namespace std;

class VNode;
class VirtualBlkPtr;


enum class BlockType { NONE, DATA, INODE, TBZ, ERROR };

struct BEntry {
    BlockType type;
    size_t size;
    char * buffer;
    uint64_t reference_count;
    bool dirty;
    std::function<void()> get_free_func() {
        return [this]() {
            reference_count--;             
        };
    };
    bool is_tbz() 
    {
        return type == BlockType::TBZ;
    }

    void release()
    {
        reference_count--;
    }

    void retain()
    {
        reference_count++;
    }
};

struct BlockCacheStat {
    size_t space; 
    size_t fspace;
};



class BlockCache {

    private:
        BEntry * tbz;
        BuddyAllocator * allocator;
    public:

        BlockCache(size_t size, int log2_block_size = DEFAULT_BLOCK_SIZE);
        BEntry * alloc(size_t size);
        void reclaim(BEntry *ptr); 
        void reclaim(CVNode * v);
        void reclaim(VirtualBlkPtr * v);
        BEntry * tbz_block();
        BlockCacheStat stats();

        std::string to_string(int tab_spacing = 0);
};

#endif
