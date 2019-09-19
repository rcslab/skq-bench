#include <stdio.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>

#include <celestis/debug.h> 
#include <celestis/OSD/util.h>
#include <celestis/OSD/diskosd.h>
#include <celestis/OSD/diskvnode.h>
#include <celestis/OSD/bitmapalloc.h>
#include <celestis/OSD/transactionmanager.h>
#include <celestis/OSD/dinode.h>

using namespace std;

STAT_TIMER(VNODER, "vnode.read", PERF_UNITS_CYCLES);
STAT_TIMER(VNODEW, "vnode.write", PERF_UNITS_CYCLES);
STAT_TIMER(VNODEC, "vnode.create", PERF_UNITS_CYCLES);
STAT_TIMER(VNODEO, "vnode.open", PERF_UNITS_CYCLES);
STAT_TIMER(VNODED, "vnode.delete", PERF_UNITS_CYCLES);

DiskOSD::DiskOSD(size_t block_cache_size, int log2_block_size)
    : block_cache_size(block_cache_size), log2_block_size(log2_block_size) {
    this->bc = new BlockCache(block_cache_size); 
}

void 
DiskOSD::unmount()
{
    this->sync();

    delete this->metanode;
    this->vnode_map.clear();
    this->metanode = nullptr;

    delete this->stg_pool;
    this->stg_pool = nullptr;

    delete this->txg_current;
    this->txg_current = nullptr;
}

vector<BEntry *>
init_entries(BlockCache * bc, vector<Disk *> disks, size_t bs)
{
    vector<BEntry *> entries;
    for (auto &d : disks) {
        size_t size = d->get_asize() / bs;
        int blocks = align_to_block(size, bs) / bs;
        for (int i = 0; i < blocks; i++) {
            BEntry * b = bc->alloc(bs);
            memset(b->buffer, '0', b->size);
            b->reference_count--;
            entries.push_back(b);
        }
    }

    return entries;
}

void
DiskOSD::mount(vector<Disk *> disks) 
{
    LOG("Mounting with disks");
    txg_current = new TransactionGroup(this);
    stg_pool = new StoragePool(this, disks, log2_block_size, false);
    mount();
    stg_pool->init_allocator(open(1), disks);

}

void
DiskOSD::initialize(vector<Disk *> disks)
{
    txg_mgr = new TransactionManager(this);
    txg_current = new TransactionGroup(this);
    stg_pool = new StoragePool(this, disks, log2_block_size, true);
    mount();
    VNode * bmap = dynamic_cast<VNode *>(stg_pool->alloc_file);
    vnode_map[1] = bmap;
    auto ptr = metanode->search((1 << log2_block_size) + 1);
    ptr->blocks.bc_entry = bmap->buffer;
}

void
DiskOSD::mount()
{
    // Grab uberblock
    DLOG("Mounting DiskOSD ");
    uberblock = this->stg_pool->get_latest_ub();

    DLOG("Mounted UB : %s", uberblock_to_str(uberblock.ub).c_str());
    BEntry *be = this->bc->alloc(1 << log2_block_size);
    int err = stg_pool->read(be->buffer, be->size, this->uberblock.ub.rootbp);
    if (err < 0) {
        PANIC();
    }

    DLOG("Read MetaObject into buffer cache ");
    metanode = new VNode(this, be, VMETA, 0);

    /*  We always have the metanode in virtual memory so have to put the
        vsize on the ptr so it doenst screw up updating our sizes. 
     */
    auto ptr = metanode->search(0);
    ptr->blocks.bc_entry = be;
    ptr->bptr.size = be->size;

    vnode_map[0] = this->metanode;
    DLOG("Mounted Metanode : %s", this->metanode->to_string().c_str());
    txg_current->txg_num = this->uberblock.ub.txg;

    // The storage pool is using a specific stub bitmap allocator now
}

CVNode *
DiskOSD::create()
{
    STAT_TSAMPLE_START(VNODEC);
    std::unique_lock<std::mutex> lock(this->vnode_map_lock);
    int i = this->metanode->find_next_empty();
    if (i < 0) {
        this->metanode->expand();
        i = this->metanode->find_next_empty();
        ASSERT(i > 0);
    }
    if (*this->metanode->asize < ((i + 1) * (1 << log2_block_size))) {
        this->metanode->truncate((i + 1) * (1 << log2_block_size));
    }
    DLOG("Creating Inode - %d", i);
    auto ptr = this->metanode->search((i * (1 << log2_block_size)) + 1);
    BEntry * bentry = this->bc->alloc(1 << log2_block_size);
    ptr->blocks.bc_entry = bentry;
    ptr->bptr.size = bentry->size;
    ASSERT(ptr->bptr.size == 1 << log2_block_size);
    ptr->dirtify();
    VNode * n = new VNode(this, bentry, VREG, i);

    // Give the cache a reference
    n->retain();
    this->vnode_map[i] = n;
    STAT_TSAMPLE_STOP(VNODEC);

    return n;
}

CVNode *
DiskOSD::check_cache(CNode i) 
{
    auto entry = this->vnode_map.find(i); 
    if (entry != this->vnode_map.end()) {
        entry->second->retain();

        return entry->second;
    }
    
    return nullptr;
}

CVNode *
DiskOSD::open(CNode i)
{
    if(this->metanode == NULL) {
        PANIC();
    }
    STAT_TSAMPLE_START(VNODEO);
    //Check cache without lock;
    auto entry = check_cache(i);
    if (entry != nullptr) {
        STAT_TSAMPLE_STOP(VNODEO);
        return entry;
    }
    {
        //Not in check -- acquire lock then check to see if its been
        //added to cache
        std::unique_lock<std::mutex> lock(this->vnode_map_lock);
        entry = check_cache(i);
        if (entry != nullptr) {
            STAT_TSAMPLE_STOP(VNODEO);
            return entry;
        }
        //Need to read from metanode
        auto bs = (1 << log2_block_size);

        VirtualBlkPtr * p = this->metanode->get_data_ptr(i);
        if (p->blocks.bc_entry == nullptr) {
            p->blocks.bc_entry = bc->alloc(bs);
        }
        SGArray read_in = SGArray();
        read_in.add(i * bs, bs, p->blocks.bc_entry->buffer);
        metanode->CVNode::read(read_in);
        VNode * n = new VNode(this, p->blocks.bc_entry, VREG, i);
        vnode_map[i] = n;
        STAT_TSAMPLE_STOP(VNODEO);
        return n;
    }
}

void
DiskOSD::sync()
{
    DLOG("Sync called");
    // We have to make sure bitmap is commited to the transaction group so we 
    // will quickly write the bitmap
    CNode bit = 1;
    CVNode * bitmap = open(bit);
    SGArray array = SGArray();
    array.add(0, bitmap->getSize(), malloc(bitmap->getSize()));
    // Read and write the bitmap to make sure we write the bitmap to the 
    // transaction
    bitmap->read(array);
    bitmap->write(array);
    bitmap->close();
    array.free();
    txg_mgr->create_new_transaction();
}

std::string
DiskOSD::to_string(int tab_spacing)
{
    std::stringstream ss;
    
    ss << std::endl;
    ss << "Object store state: " << std::endl;
    ss << uberblock_to_str(uberblock.ub, tab_spacing + 1) << std::endl;
    ss << bc->to_string(tab_spacing + 1) << std::endl;
    ss << stg_pool->to_string(tab_spacing + 1) << std::endl;
    ss << txg_mgr->to_string(tab_spacing + 1) << std::endl;
    return tab_prepend(ss , tab_spacing);
}

OSDStat
DiskOSD::stats()
{
    return OSDStat {
        .bc_size = this->bc->stats().fspace,
        .fspace = this->stg_pool->alloc->stats().fspace,
        .space = this->stg_pool->alloc->stats().space,
    };
}

void
DiskOSD::remove(CNode i) 
{
    STAT_TSAMPLE_START(VNODED);
    VNode * to_delete = dynamic_cast<VNode *> (this->open(i));
    to_delete->truncate(0);
    VirtualBlkPtr * p = this->metanode->fetch_ptr(i);
    p->status.dirty = ChangeType::DELETE;
    p->dirtify();
    txg_current->add(this->metanode);
    this->vnode_map.erase(i);
    to_delete->close();
    STAT_TSAMPLE_STOP(VNODED);
    LOG("VNode %lu removed", i);
}

void
DiskOSD::log()
{
    LOG("%s", this->to_string().c_str());
}
