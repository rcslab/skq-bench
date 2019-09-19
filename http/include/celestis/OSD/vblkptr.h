#ifndef __CELESTIS_OSD_VBLKPTR_H__
#define __CELESTIS_OSD_VBLKPTR_H__

#include <sys/types.h>
#include <vector> 
#include "blkptr.h"
#include "blockcache.h"

enum vbp_status  { VBP_NONE, VBP_READING, VBP_INCORE };
enum class ChangeType { NONE, WRITE, DELETE };

typedef vector<VirtualBlkPtr *> VPointers;

class VNode;

struct VData {
    void * data;
    size_t size;
    std::function<void()> free;    
};

class VirtualBlkPtr {
    public:
        VirtualBlkPtr(blkptr p, BlockCache *bc, VirtualBlkPtr * parent, VNode * belongs);
        VirtualBlkPtr(const VirtualBlkPtr &other);
        VirtualBlkPtr() {};
        ~VirtualBlkPtr() {};

        VirtualBlkPtr& operator=(const VirtualBlkPtr &other);
        VirtualBlkPtr& operator=(VirtualBlkPtr &&other);

	/*
	 * When calling the get_data() function you MUST call its free function 
	 * when done or else you may get uncollected BEntry buffers or memory 
	 * leaks
	 */
        VData get_data();
        void release();
        void retain();
        void clear();

        VirtualBlkPtr * get_next(int i);
        std::string to_string(int tab_spacing = 0);
        VirtualBlkPtr * fetch_ptr(int i);
        VirtualBlkPtr * next();
        VirtualBlkPtr * head();
        VirtualBlkPtr * tail();
        VirtualBlkPtr * get(int i);
        VirtualBlkPtr * search(size_t offset);
        VirtualBlkPtr * get_data_ptr(size_t i);
        void init(VirtualBlkPtr * n);

        bool is_empty();
        bool is_zeros();
        void dirtify();
        void swap(VirtualBlkPtr &other);

        struct blocks {
            BEntry * bc_entry; 
            size_t block_size;
            BlockCache * bc;
        } blocks;

        struct refs {
            VirtualBlkPtr * parent;
            VirtualBlkPtr * next;
            VirtualBlkPtr * ptrs;
            VNode * belongs;
            size_t nptrs;
            uint64_t reference_count;
        } refs;

        struct status {
            vbp_status current = VBP_NONE;
            ChangeType dirty = ChangeType::NONE;
        } status;

        blkptr bptr;
    private:
        void vPtrExpand();
    protected:
        void initList();
        void update_size();
        void update(off_t off, size_t size);
        void update();
        void rehash();
        
        friend class VNode;
        friend class TransactionManager;
        friend class BlockCache;

};

#endif
