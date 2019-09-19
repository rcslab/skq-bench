/**
 * @defgroup BitMapAllocator
 * An allocator using a simple bitmap organized using a tree class.
 * @{
 * @ingroup Allocators
 *
 * @class BitMapAllocator
 *
 * @brief A simple implementation of tree style bitmap allocator.
 *
 * @author Kenneth R Hancock
 */

#ifndef __CELESTIS_FS_BITMAPALLOC_H__
#define __CELESTIS_FS_BITMAPALLOC_H__

#include <vector>
#include <map>

#include "calloc.h" 
#include "diskalloc.h"
#include "disk.h"
#include "blockcache.h"



class BitMapAllocator : public Allocator
{
    public:

        BitMapAllocator();
        BitMapAllocator(std::vector<Disk *> disks, vector<BEntry *> blocks, int log2_block_size);
        ~BitMapAllocator() {};

        std::vector<blkptr> alloc(size_t len, blkptr p, int num_of_copies);
        void free(blkptr p);
        void expire(blkptr p);
        void flush() {};
        AllocStats stats();
        std::string to_string(int tab_spacing = 0);

    private:
        std::map<int, Allocator *> disks;
        size_t disk_space;
        std::vector<BEntry *> entries;
        std::map<std::tuple<size_t,size_t>, Disk *> range_disk;
        int log2_block_size;

        dva alloc_once(size_t len);
        void set(off_t i);

};
#endif
// @}
