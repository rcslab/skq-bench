/**
 * @defgroup BTreeAlloc
 * An allocator using a simple bitmap organized using a tree class.
 * @{
 * @ingroup Allocators
 *
 * @class BTreeAlloc
 *
 * @brief A B+ Tree Allocator for disks
 *
 * @author Kenneth R Hancock
 */

#ifndef __CELESTIS_FS_BTREEALLOC_H__
#define __CELESTIS_FS_BTREEALLOC_H__

#include <vector>
#include <unordered_map>
#include <string>
#include <fstream>
#include <iostream>

#include "disk.h"
#include "calloc.h"
#include "multibtree.h"

class BTreeAlloc : public Allocator
{
    private:

        struct sub_tree {
            BlockBackedBTree<size_t, size_t> * off;
            BlockBackedBTree<size_t, size_t> * len;
            size_t fspace;
            size_t space;
            uint16_t vdev;
        };

    private:
        size_t fspace;
        size_t space;

        MultiBTree<size_t,size_t> * trees;
        std::unordered_map<uint16_t, sub_tree> st;
        BTreeAlloc::sub_tree * get_min();
    public:
        BTreeAlloc();
        BTreeAlloc(std::vector<Disk *> &disks, CVNode * file, size_t log2_block_size);
        ~BTreeAlloc() {};
        std::vector<blkptr> alloc(size_t len, 
                blkptr p = blkptr_null(), int num_of_copies = 1);
        void free(blkptr p);
        void expire(blkptr p);
        void flush();
        AllocStats stats();
        std::string to_string(int tab_spacing = 0);
};
#endif
// @}
