#ifndef __CELESTIS_FS_VNODE_H__
#define __CELESTIS_FS_VNODE_H__

#include <shared_mutex>
#include <condition_variable>
#include <unordered_map>

#include "cvnode.h"
#include "dinode.h"
#include "blockcache.h"
#include "vblkptr.h"

#define VTYPE_INODE 0

// Forward Declaration
class StoragePool;
class DiskOSD;

class SGArray;

enum VType { VREG, VMETA, VDATA};
typedef std::unique_lock<std::shared_mutex> VPtrLock;

class VNode : public CVNode {

    public:
        int log2_block_size = 12;
        VNode(DiskOSD *os, BEntry * b, VType type, int inode_num);
        ~VNode();

        int write(SGArray &buff, std::function<void()> cb);
        int read(SGArray &sga, std::function<void()> cb);
        void truncate(size_t bytes);
        void close();
        void retain();
        void release();
        void release(size_t index);

        uint64_t _ref();
        CVNodeStat stats();
        size_t getSize();
        std::string to_string(int tab_prepend = 0);

    private:
        DiskOSD *os;

        int get_index(off_t i);
        VirtualBlkPtr * fetch_ptr(size_t offset);
        VirtualBlkPtr * search(size_t offset);

        void expand();
        void grow(size_t bytes);
        void shrink(size_t bytes);
        void execute_read(VPointers &data, 
                const SGEntry * p);
        void execute_read_layer(VPointers &data,
                VirtualBlkPtr ** layer,
                size_t n,
                size_t offset, size_t size);
    protected:
        //Variables
        bool dirty;
        dinode * inode;
        VType type;
        CNode inode_num;
        size_t * size; // Size of the file
        size_t * asize; // Bytes allocated for file
        BEntry * buffer;
        
        //Locks
        std::shared_mutex vbp_lock;
        std::condition_variable_any vbp_cond; 
        VirtualBlkPtr * ptr;

        //Functions
        void update_asize();
        void update();
        void rehash();
        blkptr get_pointer(off_t i);
        VirtualBlkPtr * get_data_ptr(uint64_t i);

        int find_next_empty();
        int inode_number();
        friend class TransactionManager;
        friend class TransactionGroup;
        friend class BlockCache;
        friend class DiskOSD;

};

#endif

