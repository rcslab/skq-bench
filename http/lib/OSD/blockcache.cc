#include <string>
#include <sstream>

#include <celestis/debug.h>
#include <celestis/pstats.h>
#include <celestis/OSD/blockcache.h>
#include <celestis/OSD/util.h>
#include <celestis/OSD/vblkptr.h>
#include <celestis/OSD/diskvnode.h>

STAT_TIMER(RECLAIM, "bc.reclaim", PERF_UNITS_CYCLES);

BlockCache::BlockCache(size_t size, int log2_block_size) {
    allocator = new BuddyAllocator(size, log2_block_size, log2_block_size, 100);
    this->tbz = new BEntry {
        .type = BlockType::TBZ,
        .reference_count = 0,
        .size = 0,
        .buffer = new char[(1 << log2_block_size)],
        .dirty = false,
    };
}

BEntry *
BlockCache::alloc(size_t size)
{

    void * buff = allocator->alloc(size);
    memset(buff, 0, size);
    return new BEntry {
        .type = BlockType::NONE,
        .reference_count = 0,
        .size = size,
        .buffer = (char *)buff,
        .dirty = false,
    };
}

void
BlockCache::reclaim(BEntry *ptr)
{
    if(ptr != this->tbz) {
        allocator->free(ptr->buffer);
    }
}

std::string
BlockCache::to_string(int tab_spacing)
{
    std::stringstream ss;
    ss << "Block cache state: " << std::endl;
    ss << "\tSize: " << stats().space << std::endl;

    return tab_prepend(ss, tab_spacing);
}

BEntry *
BlockCache::tbz_block()
{
    return this->tbz;
}

BlockCacheStat
BlockCache::stats()
{
    return BlockCacheStat {
        .space = this->allocator->stats().space,
        .fspace = this->allocator->stats().fspace
    };
}

void
BlockCache::reclaim(CVNode * v)
{
    DLOG("Reclaiming %s", v->to_string().c_str());
    STAT_TSAMPLE_START(RECLAIM);
    VNode * node = dynamic_cast<VNode *>(v);
    for (int i = 0; i < INODE_MAX_BLKPTR; i++) {
        this->reclaim(&node->ptr[i]);
    }
    STAT_TSAMPLE_STOP(RECLAIM);
    DLOG("Reclaimed %s", v->to_string().c_str());
}

void
BlockCache::reclaim(VirtualBlkPtr * v)
{
    if (v->bptr.levels > 1) {
        for (int i = 0; i < v->refs.nptrs; i++) {
            this->reclaim(&v->refs.ptrs[i]);
        }
    } else if (v->blocks.bc_entry != nullptr && v->blocks.bc_entry->reference_count == 0) {
        v->clear();
    }
}


