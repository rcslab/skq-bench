#include <string>
#include <iostream>

#include <celestis/debug.h>
#include <celestis/OSD/util.h>
#include <celestis/OSD/bitmapalloc.h>
#include <celestis/OSD/diskalloc.h>

using namespace std;

extern PerfTimer STAT_NAME(ALLOC);
extern PerfTimer STAT_NAME(FREE);


// Simplest form of an allocator right now.  Will just represent the disk as a 
// sequences of 1's and 0's. Where 1's show a block being used, 0's show a 
// block not in use.
BitMapAllocator::BitMapAllocator(vector<Disk *> disks, vector<BEntry *> entries, int log2_block_size)
    : disk_space(0), entries(entries), log2_block_size(log2_block_size)
{
    
    size_t bs = (1 << log2_block_size);
    int start = 0;

    for (auto &d : disks) {
        vector<BEntry *> sub_entries;
        size_t size = d->get_asize() / bs;
        this->disk_space += size;
        int blocks = align_to_block(size , bs) / bs;
        for (int i = start; i < start + blocks; i++) {
            BEntry * e = entries[i];
            sub_entries.push_back(e);
        }
        start += blocks;
        Allocator * disk_alloc = new DiskAllocator(d,  sub_entries, log2_block_size);
        this->disks[d->get_id()] = disk_alloc;
    }
}



std::vector<blkptr>
BitMapAllocator::alloc(size_t len, blkptr p = blkptr_null(), int num_of_copies = 1)
{
    STAT_TSAMPLE_START(ALLOC);
    Allocator * m = disks.begin()->second;
    for (auto &&a : disks) {
        if (stats().fspace < a.second->stats().fspace) {
            m = a.second;
        }
    }
    auto v = m->alloc(len, p, num_of_copies);
    STAT_TSAMPLE_STOP(ALLOC);
    return v;
}

void
BitMapAllocator::free(blkptr p)
{
    disks[p.addresses[0].vdev]->free(p);
}

void
BitMapAllocator::expire(blkptr p)
{
    disks[p.addresses[0].vdev]->expire(p);
}

std::string
BitMapAllocator::to_string(int tab_spacing)
{
    std::stringstream ss;
    ss << std::endl;

    ss << "BitMapAllocator Overall free space : " << this->stats().fspace << std::endl;
    for (auto &&d : disks) {
        ss << d.second->to_string(tab_spacing + 1) << std::endl;
    }
    return tab_prepend(ss, tab_spacing);
}


void
BitMapAllocator::set(off_t i)
{
}

AllocStats
BitMapAllocator::stats()
{
    size_t free = 0;
    for (auto &&a : disks) {
        free += a.second->stats().fspace;
    }

    size_t space = 0;
    for (auto &&a : disks) {
        space += a.second->stats().space;
    }
    
    return AllocStats {
        .fspace = free,
        .space  = space,
    };

}
