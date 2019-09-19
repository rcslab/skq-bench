#include <string>
#include <sstream>
#include <iostream>

#include <celestis/debug.h>
#include <celestis/hash.h> 
#include <celestis/OSD/util.h>
#include <celestis/OSD/storagepool.h>
#include <celestis/OSD/dinode.h>
#include <celestis/OSD/transactionmanager.h>
#include <celestis/OSD/diskvnode.h>

STAT_TIMER(SYNC, "sync", PERF_UNITS_MSEC);

TransactionManager::TransactionManager(DiskOSD * s)
    : os(s)
{
    for (int i = 0; i < COUNTDOWN; i++) {
        expiring.push(vector <blkptr> {});
    }
}


TransactionManager::~TransactionManager() 
{
}

void
TransactionManager::expire()
{
    vector<blkptr> expired = expiring.front();
    expiring.pop();
    for (auto &p : expired) {
        os->stg_pool->alloc->expire(p);
    }
    expiring.push(vector<blkptr> {});
}

void
TransactionManager::create_and_modify(VirtualBlkPtr &p, DiskMap &write_arr, dva address) 
{
        p.bptr.addresses[0] = address;
        p.status.dirty = ChangeType::NONE;
        auto d = p.get_data();
        ASSERT(d.data != nullptr);
        p.bptr.checksum = hash_data(d.data, d.size);
        p.bptr.birth_txg = this->os->txg_current->txg_num + 1;
        p.update_size();
        blkptr ptr = p.bptr;
        auto id = ptr.addresses[0].vdev;
        if (write_arr.find(id) == write_arr.end()) {
            write_arr[id] = SGArray(); 
        }
        write_arr[id].add(ptr, d);
}

void
TransactionManager::update_chain(VirtualBlkPtr &current, 
        unordered_map<uint16_t, SGArray> &write_arr, 
        bool forced) 
{
    if (current.bptr.levels > 1) {
        for (int i = 0; i < current.refs.nptrs; i++) {
            this->update_chain(current.refs.ptrs[i], write_arr);
        }
    }
    if (current.status.dirty == ChangeType::WRITE || forced) {
        Allocator * alloc = this->os->stg_pool->alloc;
        size_t size = current.bptr.levels > 1 ? 1 << os->log2_block_size : current.bptr.size;
        auto address = alloc->alloc(size)[0].addresses[0];
        create_and_modify(current, write_arr, address);
    } else if (current.status.dirty == ChangeType::DELETE) {
        blkptr bptr = current.bptr;
        current.clear();
        os->stg_pool->alloc->free(bptr);
        expiring.back().push_back(bptr);
    }
}

void
TransactionManager::update_meta(VNode * v)
{
}

void
TransactionManager::create_writes(VNode * v, DiskMap &write_arr)
{
        DLOG("Creating Writes for %s", v->to_string().c_str());
        VNode * meta = this->os->metanode;
        for (int i = 0; i < INODE_MAX_BLKPTR; i++) {
            auto change = v->ptr[i].status.dirty;
            if (change == ChangeType::WRITE || change == ChangeType::DELETE) {
                this->update_chain(v->ptr[i], write_arr);
            }
        } 
        auto ptr = meta->search((v->inode_num * (1 << os->log2_block_size)));
        if (v != meta) {
            ptr->dirtify();
        }
        v->update();
}

string
TransactionManager::to_string(int tab_spacing)
{
    std::stringstream ss;
    ss << this->os->txg_current->to_string() << std::endl;
    return tab_prepend(ss, tab_spacing);
}

DiskMap
TransactionManager::write_inodes()
{
    // Update pointers and creates new metanode
    DLOG("TXG %lu collecting dirty inodes", this->os->txg_current->txg_num);
    VNode * meta = this->os->metanode;
    unordered_map<VirtualBlkPtr *, dva> allocation;
    DiskMap write_arr;
    for (auto &it : this->os->txg_current->dirty) {
        if (it->inode_number() > 1) {
            this->create_writes(it, write_arr);        
        }
    }
    this->create_writes(meta, write_arr);

    for (int i = INODE_MAX_BLKPTR - 1; i > 1; i--) {
        this->update_chain(meta->ptr[i], write_arr);
    }

    return write_arr;
}

DiskMap
TransactionManager::write_reserved()
{
    LOG("Writing bitmap");
    auto bs = 1 << os->log2_block_size;
    auto future_alloc = os->stg_pool->alloc->alloc(bs)[0].addresses[0];
    VNode * bmap = dynamic_cast<VNode *> (this->os->open(1));
    DiskMap write_arr;
    os->stg_pool->alloc->flush();
    create_writes(bmap, write_arr);
    // We have to rehash as we've allocated and need to make sure to have the 
    // most up to date hashes
    bmap->rehash();
    auto ptr = os->metanode->search(bmap->inode_num * bs);
    create_and_modify(*ptr, write_arr, future_alloc);
    bmap->update();
    return write_arr;
}

void disk_map_extend(DiskMap &a,DiskMap &b)
{
    for (auto &&e : b) {
        if(a.find(e.first) == a.end()) {
            a[e.first] = e.second;
        } else {
            a[e.first].add(e.second);
        }
    }
}
void
TransactionManager::create_new_transaction()
{
    STAT_TSAMPLE_START(SYNC);
    // Acquire the transaction lock
    auto bs = 1 << os->log2_block_size;
    Allocator * alloc = os->stg_pool->alloc;

    std::unique_lock<std::mutex> lock(os->txg_lock);
    expire(); 

    uint64_t current_txg_num = this->os->txg_current->txg_num;
    LOG("TXG %lu Starting new transaction", current_txg_num);
    VNode * meta = os->metanode;
    // Allocate for metanode
    ub_helper next = os->stg_pool->get_next_ub(os->uberblock);
    next.ub.rootbp.addresses[0] = alloc->alloc(1 << os->log2_block_size)[0].addresses[0];
    next.ub.rootbp.size = bs;

    auto dm = write_inodes();
    auto dm_b = write_reserved();
    disk_map_extend(dm, dm_b);

    // Have to save the level. Should take this into account when allocating. Maybe give past bptr?
    auto ptr_zero = meta->search(0);
    ptr_zero->bptr = next.ub.rootbp;
    meta->update();
    next.ub.rootbp.checksum =  hash_data(meta->inode, bs);
    VData v = ptr_zero->get_data();
    ASSERT(v.data != nullptr);
    dm[next.ub.rootbp.addresses[0].vdev].add(next.ub.rootbp, v);
    ssize_t err = os->stg_pool->write(dm); 
    if (err < 0) {
        PANIC();
    }
    next.ub.txg = current_txg_num + 1;
    // Update uberblock will hash the ub so we need to grab the latest after 
    // calling this
    this->os->stg_pool->update_uberblock(next);
    DLOG("TXG Clean up");
    //for(auto &it : *this->os->txg_current) {
        //it.first->close();
        //if (it.first->_ref() == 0) {
            //meta->release(it.first->inode_num);
            //this->os->bc->reclaim(it.first);
        //}
    //}
    os->bc->reclaim(meta);

    delete os->txg_current;
    os->txg_current = new TransactionGroup(this->os);
    os->txg_current->txg_num = current_txg_num + 1;

    os->uberblock = next;
    STAT_TSAMPLE_STOP(SYNC);
}
