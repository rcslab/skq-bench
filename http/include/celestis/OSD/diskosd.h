/**
 * @file objectstore.h
 * @{
 * @class DiskOSD
 *
 * @ingroup ObjectStoreDevice
 *
 * ObjectStore is the main abstraction for handling data object, it handles 
 * instantiating the Storage pool, Block Cache, and the Transaction manager.  
 *
 * Object store acts as a way to retrieve VNode objects who's API allows for 
 * reads and writes to its block cache entries.  The object store holds onto 
 * the metanode VNode which acts as the root of the VNode tree. Where its data 
 * leafs are other VNodes, rather then data itself. 
 *
 * Syncing will cause a flush of dirty block cache entries within the 
 * transaction group to flush to disk
 *
 * @author Kenneth R Hancock
 */

#ifndef __CELESTIS_FS_DISKOSD_H__
#define __CELESTIS_FS_DISKOSD_H__

#include <string>
#include <vector>
#include <unordered_map>
#include <condition_variable>

#include "calloc.h"
#include "cvnode.h"
#include "cosd.h"
#include "defines.h"
#include "disk.h"
#include "storagepool.h"
#include "../pstats.h"
#include "../threadpool.h"

// Forward Dec 
class TransactionGroup;
class TransactionManager;


class DiskOSD : public CObjectStore {
    public:
        /**
         * Main constructor of the object store
         *
         * @param block_cache_size Number of memory blocks to allocate.
         * @param log2_block_size The size of blocks for this ObjectStore 
         * device
         */
        DiskOSD(size_t block_cache_size = DEFAULT_CACHE_SIZE, int log2_block_size = DEFAULT_BLOCK_SIZE);
        ~DiskOSD() {};
        
        CVNode * open(CNode i);
        CVNode * create();
        void remove(CNode i);
        OSDStat stats();
        void sync();


        /**
         * mount will remount disks that have already been initialized.  This 
         * function can be called to remount disks after an unmount.
         *
         * @note Order currently matters for disks being mounted. Meaning you 
         * cannot have a different order from when you initialized the disks.
         *
         * @param disk The disks to mount.
         */
        void mount(vector<Disk *> disks);

        /**
         * intiailize will mount disks for the first time
         *
         * @param disks The disks to mount
         */
        void initialize(vector<Disk *> disks);

        /**
         * unmount will sync current state onto the disks and allow for 
         * ejection of disks from the ObjectStore.
         */
        void unmount();
        void log();
        std::string to_string(int tab_spacing = 0);

    private:
        size_t block_cache_size;
        void mount();

        CVNode * check_cache(CNode i);
    protected:
        int log2_block_size;
        ub_loc uberblock;
        VNode * metanode;
        BlockCache * bc;
        unordered_map<uint64_t, VNode *> vnode_map;

        StoragePool * stg_pool;
        TransactionManager * txg_mgr;
        TransactionGroup * txg_current;
        
        std::condition_variable_any txg_cond;
        std::mutex txg_lock;
        std::mutex vnode_map_lock;

        friend class VNode;
        friend class TransactionManager;
        friend class TransactionGroup;
        friend class StoragePool;

};
#endif 
// @}
