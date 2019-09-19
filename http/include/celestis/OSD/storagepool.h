/*
 * @ingroup ObjectStoreDevice
 * 
 * @class StoragePool
 */
#ifndef __CELESTIS_FS_STORAGEPOOL_H__
#define __CELESTIS_FS_STORAGEPOOL_H__

#include <vector>
#include <unordered_map>

#include "disk.h"
#include "calloc.h"
#include "uberblock.h"
#include "blockcache.h"
#include "cvnode.h"
#include "../threadpool.h"

typedef struct ub_loc {
    off_t offset = -1;
    int disk_num;
    uberblock ub;
} ub_helper;

struct write_e { 
    VirtualBlkPtr * base;
    size_t len;
    void * reference;
};

typedef unordered_map<uint16_t, SGArray> DiskMap;

class DiskOSD;

class StoragePool {
    private:
        std::unordered_map<uint32_t, Disk *> disks;
        int block_size;
        BlockCache * bc;
        void create_pool(CVNode * alloc);
        Disk * get_disk(uint32_t id);
        Celestis::ThreadPool pool = Celestis::ThreadPool();
    protected:
        void update_uberblock(ub_helper &ub_h);
        friend class TransactionManager;
    public:
        StoragePool(DiskOSD * os, std::vector<Disk *> disks,
                int block_size = DEFAULT_BLOCK_SIZE, bool init = true);

        ~StoragePool() {};

        CVNode * alloc_file;
        Allocator * alloc;
        ub_loc get_latest_ub();
        ub_loc get_next_ub(ub_loc &ub_l);
        void init_allocator(CVNode * file, std::vector<Disk *> &disks);
        static void init_disk(Disk *d);
        static void init_disks(std::vector<Disk *> disks);

        ssize_t write(char * buffer, size_t len, blkptr ptr);
        ssize_t write(char * buffer, size_t disk, size_t len, off_t offset);
        int write(DiskMap &disksga);
        int read(char * dst, size_t len, blkptr ptr);
        int read(DiskMap &disksga);
        size_t space();

        BEntry* get_block(VNode *v, blkptr ptr);

        std::string to_string(int tab_spacing = 0);

};
#endif

