/**
 * @ingroup DataStructures
 *
 * @class MultiBTree
 *
 * @brief Using one CVNode to control many BTrees, used within our BTree 
 * Allocation strategy
 *
 * @author Kenneth R Hancock
 */

#ifndef __CELESTIS_FS_MULTIBTREE_H__
#define __CELESTIS_FS_MULTIBTREE_H__ 

#include <celestis/debug.h>
#include <celestis/blockbackedbtree.h>

extern PerfTimer STAT_NAME(BTREEI);
extern PerfTimer STAT_NAME(BTREED);
extern PerfTimer STAT_NAME(BTREEG);

template<typename _K, typename _V>
class MultiBTree 
{
    private:
        std::vector<BlockBackedBTree<_K,_V> *>trees;
        BlockAlloc bc = BlockAlloc();
        size_t bs;
    public:
        MultiBTree<_K, _V>(CVNode * file, size_t num, size_t log2_block_size)
        {
            bs = 1 << log2_block_size;
            bc.init(file, log2_block_size);
            size_t required_size = (num + 1) * bs;
            auto stats = file->stats();
            if (stats.size == bs) {
                for (size_t i = 0; i < num; i++) {
                    trees.push_back(new BlockBackedBTree<_K, _V>(&bc, bc.alloc()));
                }
            // The roots should be at blocks 1...num
            } else if (stats.size >= required_size) {
                std::vector<size_t> ids;
                for (size_t i = 1; i < num + 1; i++) {
                    ids.push_back(i); 
                }
                auto buffers = bc.getv(ids);
                for (auto buff : buffers) {
                    trees.push_back(new BlockBackedBTree<_K, _V>(&bc, buff));
                }
            } else {
                PANIC();
            }
        }

        void flush()
        {
            bc.flush();
        }

        BlockBackedBTree<_K,_V> * operator[](int num)
        {
           return trees[num];
        }

        BlockBackedBTree<_K,_V> * get(int num)
        {
           return trees[num];
        }


        size_t size()
        {
            return trees.size();
        }
};
#endif
