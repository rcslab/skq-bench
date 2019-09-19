/**
 * @ingroup Allocators
 *
 * @class DiskAllocator
 *
 * @brief A simple implementation of tree style bitmap allocator.
 *
 * @author Kenneth R Hancock
 */

#ifndef __CELESTIS_FS_DISKALLOC_H__
#define __CELESTIS_FS_DISKALLOC_H__

#include <vector>
#include <map>

#include "calloc.h" 
#include "disk.h"
#include "blockcache.h"

class DiskAllocator : public Allocator
{
    public:
        DiskAllocator(Disk * disk, vector<BEntry *> entries, int log2_block_size);

        std::vector<blkptr> alloc(size_t len, blkptr p, int num_of_copies);
        void free(blkptr p);
        void expire(blkptr p);
        void flush () {};
        AllocStats stats();
        std::string to_string(int tab_spacing = 0);
    private:
        Disk * disk;
        vector<BEntry *> entries;

	int log2_block_size;
        size_t disk_space;
        size_t free_disk_space;

        dva alloc_once(size_t len);
        void set(blkptr p, char c);
};
#endif
// @}
