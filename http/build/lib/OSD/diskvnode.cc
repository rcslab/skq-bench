#include <algorithm>
#include <string>
#include <sstream> 
#include <unordered_map> 
#include <unordered_set>
#include <chrono>
#include <thread>

#include <strings.h>
#include <math.h>

#include <celestis/debug.h> 
#include <celestis/OSD/diskvnode.h>
#include <celestis/OSD/diskosd.h>
#include <celestis/OSD/util.h>
#include <celestis/OSD/transactionmanager.h>
#include <celestis/OSD/dva.h>


#define NEGATIVE_SINDIRECT_OFFSET (3)
#define NEGATIVE_DINDIRECT_OFFSET (NEGATIVE_SINDIRECT_OFFSET - 1)
#define NEGATIVE_TINDIRECT_OFFSET (NEGATIVE_DINDIRECT_OFFSET - 1)
#define SINDIRECT (INODE_MAX_BLKPTR - NEGATIVE_SINDIRECT_OFFSET)
#define DINDIRECT (INODE_MAX_BLKPTR - NEGATIVE_DINDIRECT_OFFSET)
#define TINDIRECT (INODE_MAX_BLKPTR - NEGATIVE_TINDIRECT_OFFSET)
#define NUM_OF_DATA_BLOCKS (INODE_MAX_BLKPTR - 3)

#define MIN(a,b) (((a)<(b))?(a):(b))

extern PerfTimer STAT_NAME(VNODER);
extern PerfTimer STAT_NAME(VNODEW);


VNode::VNode(DiskOSD *os, BEntry * b, VType type, int inode_num) 
    : os(os), type(type), inode_num(inode_num),  buffer(b)
{
    inode = (dinode *)b->buffer;
    // We allocate more because when the pointer has to expand we will just reuse
    // the already allocated memory - this serves also to preserve parent child
    // relationships within the pointer trees.
    size_t max = (1 << log2_block_size) / sizeof(blkptr);
    ptr = new VirtualBlkPtr[max];
    for (int i = 0; i < INODE_MAX_BLKPTR; i++) {
        if (inode->blkptr[i].levels == 0) {
            inode->blkptr[i].levels = 1;
        }
        ptr[i] = VirtualBlkPtr(inode->blkptr[i], os->bc, nullptr, this);

    }
    for (int i = 0; i < INODE_MAX_BLKPTR - 1; i++) {
        ptr[i].init(&ptr[i + 1]);
    }

    asize = &inode->asize;
    size = &inode->size;
    retain();
}

VNode::~VNode()
{
}

// Can I lose the next and parent pointer all together and just reshape how I look at truncate
// Right now moving thing around is complicated.  What if truncate just sets the size and
// read takes care of the rest.  Once I read then Ill figure out what pointers are getting what
// based on gaps  It seems like current method is getting too complicated too keep track 
// of pointers in the tree.
//
// The problem is copying into a vector create a copy so the stack ends up being different 
// and its location is different.  So all the next pointers are screwed when expand occurs.
void
VNode::shrink(size_t bytes)
{
    auto current = search(bytes);
    auto node = current;
    size_t size = bytes - node->bptr.offset;
    auto entry = node->blocks.bc_entry;
    node->bptr.size = size;
    if (size == 0) {
        current->dirtify();
        current->status.dirty = ChangeType::DELETE;
        node->blocks.bc_entry = nullptr;
    } else {
        node->dirtify();
        node->status.dirty = ChangeType::WRITE;
        node->blocks.bc_entry = os->bc->alloc(size);
        memcpy(node->blocks.bc_entry->buffer, entry->buffer, size);
    }
    os->bc->reclaim(entry);
      
    current = current->next();
    if (current->bptr.offset != 0) {
        for (size_t i = current->bptr.offset ; i < size;) {
            current->dirtify();
            current->status.dirty = ChangeType::DELETE;
            i += current->bptr.size;
            current = current->next();
        }
    }
    os->txg_current->add(this);
}

VirtualBlkPtr *
VNode::get_data_ptr(uint64_t i) 
{
    size_t max = (1 << log2_block_size) / sizeof(blkptr);
    if (ptr[0].bptr.levels == 1) {
        if (i > INODE_MAX_BLKPTR) {

            return nullptr;
        } 

        return &ptr[i];
    } else {
        auto t = (uint64_t)pow((double)max, (double)(ptr[0].bptr.levels - 1));
        auto node = i / t;
        auto left = i % t;

        return ptr[node].get_data_ptr(left);
    }
}

void
VNode::expand()
{
    auto topPtr = blkptr_null();
    topPtr.levels = ptr[0].bptr.levels + 1;
    topPtr.offset = ptr[0].bptr.offset;
    auto size = 0;
    // Push old pointers on this new 
    auto bottomPtr = blkptr_null();
    bottomPtr.levels = topPtr.levels - 1;
    bottomPtr.size = 0;
    bottomPtr.offset = 0;

    size_t max = (1 << log2_block_size) / sizeof(blkptr);
    auto new_ptrs = new VirtualBlkPtr[max];
    for (int i = 0; i < INODE_MAX_BLKPTR; i++) {
        new_ptrs[i] = VirtualBlkPtr(topPtr, os->bc, nullptr, this);
    }
    new_ptrs[0].refs.ptrs = &ptr[0];
    new_ptrs[0].refs.nptrs = max;
    for (int i = 0; i < INODE_MAX_BLKPTR; i++) {
        size += ptr[i].bptr.size;
        ptr[i].refs.parent = &new_ptrs[0];
    }
    new_ptrs[0].bptr.size = size;

    for (int i = 0; i < INODE_MAX_BLKPTR - 1; i++) {
        new_ptrs[i].init(&new_ptrs[i + 1]);
    }
    
    for (int i = INODE_MAX_BLKPTR; i < max; i++) {
        new_ptrs[0].refs.ptrs[i] = VirtualBlkPtr(bottomPtr, os->bc, &new_ptrs[0], this);
    }

    for (int i = 0; i < max - 1; i++) {
        new_ptrs[0].refs.ptrs[i].init(&new_ptrs[0].refs.ptrs[i + 1]);
    }
    ptr = new_ptrs;
}

void
VNode::truncate(size_t bytes)
{
    VPtrLock lock(vbp_lock);
    // Physically needs to shrink on disk
    if (bytes < *size) {
        shrink(bytes);
    } else {
        // We want the last data block
        size_t size = getSize() ? getSize() -1 : 0;
        auto current = search(size);
        if (current->is_zeros()) {
            current->bptr.size = bytes - current->bptr.offset;
            current->update(current->bptr.offset, bytes - current->bptr.offset);
        } else {
            current = current->next();
            ASSERT(current->is_zeros());
            current->update(getSize(), bytes - getSize());
        }
    }
    *size = bytes;
    os->txg_current->add(this);
}


VirtualBlkPtr *
VNode::fetch_ptr(size_t offset)
{
    for (int i = 0; i < INODE_MAX_BLKPTR; i++) {
        bool lower = offset >= ptr[i].bptr.offset;
        size_t size = ptr[i].bptr.size ? ptr[i].bptr.size - 1 : 0;
        bool upper = offset <= ptr[i].bptr.offset + size;
        // We check to see if the next one is empty then it must be this pointer
        // we need to dive into
        if ((lower && upper) || (ptr[i + 1].bptr.offset == 0)) {

            return &ptr[i];
        }
    }
    PANIC();
    return nullptr;
};

VirtualBlkPtr *
VNode::search(size_t offset)
{
    return fetch_ptr(offset)->search(offset);
}

void
trim(VPointers &data, size_t lower, size_t upper)
{

    for (int i = 0; i < data.size(); i++) {
        auto p = data[i];
        if ((p->bptr.offset + p->bptr.size) < lower) {
            data.erase(data.begin());
        } else {
            break;
        }
    }
    
    for (int i = data.size() - 1; i <= 0; i--) {
        auto p = data[i];
        if ((p->bptr.offset) > upper) {
            data.pop_back();
        } else {
            break;
        }
    }
}

void
VNode::execute_read(VPointers &data, const SGEntry * p)
{
    ASSERT (p->offset + p->len <= getSize());
    vector<VirtualBlkPtr *> pointers;
    for (int i = 0; i < INODE_MAX_BLKPTR; i++) {
        pointers.push_back(&ptr[i]);
    }
    execute_read_layer(data, &pointers[0], 
            INODE_MAX_BLKPTR, p->offset, p->len);
}

void
VNode::execute_read_layer(VPointers &data, 
        VirtualBlkPtr ** layer, size_t n, size_t offset, size_t size)
{
    if (!n) {
        
        return;
    }
    VPointers filtered_layer;
    VPtrLock lock(vbp_lock);
    for (int i = 0; i < n; i++) {
        auto current_ptr = layer[i];
        auto lower = current_ptr->bptr.offset;
        auto upper = lower + current_ptr->bptr.size;
        auto check_upper = upper > offset && upper <= offset + size;
        auto check_lower = lower >= offset && lower < offset + size;
        auto check_encap = lower <= offset && upper >= offset + size;

        if ((check_upper || check_lower || check_encap) && (upper != lower)) {
           filtered_layer.push_back(current_ptr);
        }
    }

    DiskMap diskarr;
    for (auto &ptr : filtered_layer) {
        if (ptr->status.current == VBP_READING) {
            this->vbp_cond.wait(lock, [ptr]{ return ptr->status.current != VBP_READING; });
        } else if (ptr->status.current == VBP_NONE) {
            auto id = ptr->bptr.addresses[0].vdev;
            auto offset = ptr->bptr.addresses[0].offset;
            auto size = ptr->bptr.addresses[0].asize;
            if (offset == static_cast<uint64_t>(-1)) {
            }
            if (size != 0) {
                ptr->status.current = VBP_READING;
                auto b = os->bc->alloc(ptr->bptr.size);
                ptr->blocks.bc_entry = b;
                if (diskarr.find(id) == diskarr.end()) {
                    diskarr[id] = SGArray();
                }
                auto f = [ptr] () {
                    ptr->status.current = VBP_INCORE;
                };
                diskarr[id].add(ptr->bptr.addresses[0].asize, 
                    ptr->bptr.addresses[0].offset,
                    b->buffer, f,
                    b);
            }
        }
    }

    auto r = os->stg_pool->read(diskarr);
    if (r) {
        ASSERT(false);
    }
    VPointers next_layer;
    for (auto &ptr : filtered_layer) {
        if (ptr->bptr.levels == 1) {
            data.push_back(ptr);
        } else {
            for (int i = 0; i < ptr->refs.nptrs; i++) {
                ptr->initList();
                next_layer.push_back(&ptr->refs.ptrs[i]);
            }
        }
    }
    this->vbp_cond.notify_all();
    lock.unlock();
    execute_read_layer(data, &next_layer[0], next_layer.size(), offset, size);
}


int
VNode::read(SGArray &sga, std::function<void()> cb)
{
    STAT_TSAMPLE_START(VNODER);
    for (auto k = 0; k < sga.size(); k++) {

        VPointers data;

        auto p = sga.get(k);
        ASSERT(p->buffer != nullptr);

        execute_read(data, p);
        char * to_buffer = (char *)p->buffer;
        size_t local_off = p->offset - data[0]->bptr.offset;
        size_t left = p->len;
        for (auto &ptr : data) {
            size_t size_of_cp;
            if (ptr->is_zeros()) {
                size_of_cp = MIN(left, ptr->bptr.size - local_off);
                memset(to_buffer, 0, size_of_cp);
            } else {
                char * from_buffer = (char *)ptr->blocks.bc_entry->buffer + local_off;
                size_of_cp = MIN(left, ptr->blocks.bc_entry->size - local_off);
                ASSERT(size_of_cp != 0);
                memcpy(to_buffer, from_buffer, size_of_cp);
            }
            to_buffer += size_of_cp;
            left -= size_of_cp;
        }
        ASSERT (left == 0);
    }
    cb();
    STAT_TSAMPLE_STOP(VNODER);
    return 0;
}


int
VNode::write(SGArray &sga, std::function<void()> cb) 
{
    STAT_TSAMPLE_START(VNODEW);
    for (auto k = 0; k < sga.size(); k++) {
        auto p = sga.get(k);
        vector<VirtualBlkPtr *> data;
        execute_read(data, p);
        VPtrLock lock(vbp_lock);
        char * from_buffer = (char *)p->buffer;
        size_t local_off = p->offset - data[0]->bptr.offset;
        size_t left = p->len;
        for (auto &ptr : data) {
            if (ptr->is_zeros()) {
                ptr->blocks.bc_entry = os->bc->alloc(ptr->bptr.size);
            }
            char * to_buffer = (char *)ptr->blocks.bc_entry->buffer + local_off;
            size_t size_of_cp = MIN(left, ptr->blocks.bc_entry->size - local_off);
            memcpy(to_buffer, from_buffer, size_of_cp);
            ptr->dirtify();
            from_buffer += size_of_cp;
            left -= size_of_cp;
        }
        ASSERT (left == 0);
    }
    os->txg_current->add(this);
    STAT_TSAMPLE_STOP(VNODEW);
    return 0;
}

int
VNode::get_index(off_t i)
{
    size_t size = 0;
    int index;
    for (index = 0; index < this->inode->numbp; index++) {
        size += INODE_ON_DISK_SIZE;
        if(i < size) {
            break;
        }
    }

    return index;
}

blkptr
VNode::get_pointer(off_t i)
{
    return inode->blkptr[get_index(i)];
}

std::string
VNode::to_string(int tab_spacing)
{
    std::stringstream ss;
    ss << "VNode " << this->inode_num << std::endl;
    ss << "\tReference Count: " << this->_ref() << std::endl;

    return tab_prepend(ss, tab_spacing);
}

CVNodeStat
VNode::stats()
{
    return CVNodeStat {
        .size = *size,
        .asize = *asize,
        .inum = inode_num
    };
}

int
VNode::inode_number()
{
    return this->inode_num;
}

void
VNode::update()
{
    dinode * t = inode;
    update_asize();
    for (int i = 0; i < INODE_MAX_BLKPTR; i++) {
        blkptr p = ptr[i].bptr;
        memcpy(&t->blkptr[i], &p, sizeof(struct blkptr));
    }
}

int
VNode::find_next_empty()
{
    size_t count = 0;
    VirtualBlkPtr * p = this->ptr[0].head();
    while (p != nullptr) {
        if (p->is_empty()) {

            return count;
        } else {
            count++;
            ASSERT(p->bptr.size == 4096);
            p = p->next();
        }
    }

    return -1;
}

void
VNode::close()
{
    release();
    if (!_ref()) {
        for (int i = 0; i < INODE_MAX_BLKPTR; i++) {
            ptr[i].release();
        }
    }
}

size_t
VNode::getSize()
{
    //File is only on disk
    if (*asize > 0 && *size == 0){
        return *asize;
    }
    //Return the the file size asked for
    return *size;
}

void
VNode::update_asize()
{
    *asize = 0;
    for (int i = 0; i < INODE_MAX_BLKPTR; i++) {
        if(!ptr[i].is_empty()) {
            *asize += ptr[i].bptr.addresses[0].asize;
        }
    }
}

void
VNode::release()
{
    buffer->reference_count--;
}

void
VNode::retain()
{
    buffer->reference_count++;
}

uint64_t
VNode::_ref()
{
    return buffer->reference_count;
}

void
VNode::release(size_t index)
{
    std::unique_lock<std::shared_mutex> lock(this->vbp_lock);
    VirtualBlkPtr * p = this->fetch_ptr(index);
    p->release();
}


void
VNode::rehash()
{
    for (int i = 0; i < INODE_MAX_BLKPTR; i++) {
        if (!ptr[i].is_empty()) {
            ptr[i].rehash();
        }
    }
}
