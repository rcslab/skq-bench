#include <math.h>

#include <celestis/debug.h>
#include <celestis/OSD/util.h>
#include <celestis/OSD/defines.h>
#include <celestis/OSD/vblkptr.h>
#include <celestis/OSD/diskvnode.h>

VirtualBlkPtr::VirtualBlkPtr(blkptr p, BlockCache * bc, VirtualBlkPtr * parent, VNode * belongs) : bptr(p)
{
    blocks.block_size = 4096;
    blocks.bc_entry = nullptr;
    blocks.bc = bc;
    refs.parent = parent;
    refs.nptrs = 0;
    refs.next = nullptr;
    refs.ptrs = nullptr;
    refs.belongs = belongs;
    refs.reference_count = 0;
}

VirtualBlkPtr::VirtualBlkPtr(const VirtualBlkPtr &other) : blocks(other.blocks), 
    refs(other.refs), status(other.status), bptr(other.bptr)
{}

VirtualBlkPtr& 
VirtualBlkPtr::operator=(const VirtualBlkPtr &other) 
{
    auto v = VirtualBlkPtr(other);
    swap(v);
    return *this;
}

VirtualBlkPtr& 
VirtualBlkPtr::operator=(VirtualBlkPtr &&other)
{
    swap(other);
    return *this;
}

void
VirtualBlkPtr::swap(VirtualBlkPtr &other)
{
    std::swap(refs, other.refs);
    std::swap(blocks, other.blocks);
    std::swap(status, other.status);
    std::swap(bptr, other.bptr);
}

VirtualBlkPtr *
VirtualBlkPtr::head()
{
    if(bptr.levels <= 1) {
        return this;
    } 
    return get(0)->head();
}
VirtualBlkPtr *
VirtualBlkPtr::get_data_ptr(uint64_t i) 
{
    if (bptr.levels == 2) {
        initList();

        return &refs.ptrs[i];
    } else {
        double max = blocks.block_size / sizeof(blkptr);
        auto t = (uint64_t)pow((double)max, (double)(bptr.levels - 2));
        size_t node = i / t;
        size_t left = i % t;

        return refs.ptrs[node].get_data_ptr(left);
    }
}


void
VirtualBlkPtr::initList()
{
    if (refs.nptrs) {

        return;
    }
    double max = blocks.block_size / sizeof(blkptr);
    auto b = blkptr_null();
    b.levels = bptr.levels - 1;
    refs.ptrs = new VirtualBlkPtr[max];
    refs.nptrs = max;
    for (int i = 0; i < max; i++) {
        refs.ptrs[i] = VirtualBlkPtr(b, blocks.bc, this, refs.belongs);
    }
    for (int i = 0; i < max - 1; i++) {
        refs.ptrs[i].init(&refs.ptrs[i + 1]);
    }
}

VirtualBlkPtr *
VirtualBlkPtr::get(int i)
{
    size_t indirect_size = (1 << blocks.block_size) / sizeof(blkptr);
    if (i > indirect_size || bptr.levels == 1) {
        PANIC();
    }

    if(!refs.nptrs) {
        initList();
    }

    return &refs.ptrs[i];
}

void
VirtualBlkPtr::init(VirtualBlkPtr * n) 
{
    ASSERT(n != nullptr);
    ASSERT(n->bptr.levels == bptr.levels);
    refs.next = n;
}

void
VirtualBlkPtr::vPtrExpand() {
    if (refs.parent != nullptr) {
        refs.parent->vPtrExpand();
    }
    if (refs.parent != nullptr && refs.parent->next() != nullptr) {
        init(refs.parent->next()->get(0));
    }
}

VirtualBlkPtr *
VirtualBlkPtr::next()
{
    if (refs.next == nullptr) {
        if (refs.parent != nullptr) {
            vPtrExpand();
        }
        return refs.next;
    }

    return refs.next;
}

VirtualBlkPtr *
VirtualBlkPtr::tail()
{
    if(bptr.levels == 1) {
        return this;
    } 
    return get(refs.nptrs - 1)->tail();
}


VirtualBlkPtr *
VirtualBlkPtr::get_next(int i)
{
    if (i < 0) {
        throw "UH OH";
    }
    if (i == 0) {
        return this;
    }
    return nullptr;
};

VirtualBlkPtr *
VirtualBlkPtr::fetch_ptr(int i) 
{
    if (bptr.levels == 1) {
        ASSERT (i == 0);
        return this;
    }
    return get_next(i);
};

std::string
VirtualBlkPtr::to_string(int tab_spacing)
{
    std::stringstream ss;
    
    ss << "VirtualBlkPtr: "<< std::endl;
    ss << "\t.levels: " << bptr.levels << std::endl;
    ss << "\t.size: " << bptr.size << std::endl;
    ss << "\t.addresses vdev: " << bptr.addresses[0].vdev << std::endl;
    ss << "\t.addresses offset: " << bptr.addresses[0].offset << std::endl;
    ss << "\t.addresses len: " << bptr.addresses[0].asize << std::endl;
    return tab_prepend(ss , tab_spacing);
}

void
update_size(VirtualBlkPtr * current, size_t size) 
{
    if (current == nullptr) {
        return;
    }

    current->bptr.size += size; 
    update_size(current->refs.parent, size);
}

// TODO THIS IS AWFUL
// POP A BC ENTRY OFF AND USE THAT AS THE BUFFER IF YOU HAVE TO
VData
VirtualBlkPtr::get_data()
{
    size_t max = blocks.block_size / sizeof(blkptr);
    if(bptr.levels > 1) {
        blkptr * data_pointers = new blkptr[max];
        for(int i = 0; i < refs.nptrs; i++) {
            data_pointers[i] = refs.ptrs[i].bptr;
        }
        return VData {
	    .size = sizeof(blkptr) * max,
            .data = data_pointers,
            .free = [data_pointers]() {
                delete[] data_pointers;
            }
        };
    }

    if (blocks.bc_entry == nullptr) {
        return VData {
	    .size = 0,
            .data = nullptr,
            .free = 0
        };
    }
    blocks.bc_entry->reference_count++;
    return VData {
	.size = blocks.bc_entry->size,
        .data = blocks.bc_entry->buffer,
        .free = blocks.bc_entry->get_free_func()
    };
}

void
VirtualBlkPtr::release()
{
    if(bptr.levels > 1) {
        for (int i = 0; i < refs.nptrs; i++) {
            refs.ptrs[i].release();
        }
    } else if (blocks.bc_entry != nullptr && blocks.bc_entry->type != BlockType::TBZ) {
        blocks.bc_entry->reference_count--;
    }
}

void
VirtualBlkPtr::retain()
{
    if(bptr.levels > 1) {
        for (int i = 0; i < refs.nptrs; i++) {
            refs.ptrs[i].retain();
        }
    } else if (blocks.bc_entry != nullptr && blocks.bc_entry->type != BlockType::TBZ) {
        blocks.bc_entry->reference_count++;
    }
}

void
VirtualBlkPtr::update(off_t offset, size_t size)
{
    bptr.offset = offset;
    int d_size = size - bptr.size;
    bptr.size = size;
    if (refs.parent != nullptr) {
        if (&refs.parent->refs.ptrs[0] == this) {
            refs.parent->bptr.offset = offset;
        }
        auto off = refs.parent->bptr.offset;
        refs.parent->update(off, refs.parent->bptr.size + d_size);
    }
}

void
VirtualBlkPtr::update_size()
{
    if (bptr.levels == 1) {
        return;
    }
    bptr.size = 0;
    for (int i = 0; i < refs.nptrs; i++) {
        VirtualBlkPtr &p = refs.ptrs[i];
        p.update_size();
        bptr.size += p.bptr.size;
    }
}

void
VirtualBlkPtr::clear()
{
    if (blocks.bc_entry != nullptr) {
        blocks.bc_entry->reference_count--;
        blocks.bc->reclaim(blocks.bc_entry);
    }
    delete refs.ptrs;
    refs.nptrs = 0;
    status.current = VBP_NONE;
    blocks.bc_entry = nullptr;
    bptr.size = 0;
    bptr.checksum = Hash {};
    bptr.addresses[0] = dva_null();
    bptr.addresses[1] = dva_null();
    bptr.addresses[2] = dva_null();
}

void
VirtualBlkPtr::dirtify()
{
    status.dirty = ChangeType::WRITE;
    if (refs.parent != nullptr && refs.parent->status.dirty == ChangeType::NONE) {
        refs.parent->dirtify();
    }
}

bool
VirtualBlkPtr::is_empty()
{
    return (refs.nptrs == 0) && (blocks.bc_entry == nullptr) && 
        (bptr.size == 0);
}

void
VirtualBlkPtr::rehash() 
{
    for (int i = 0; i < refs.nptrs; i++) {
        refs.ptrs[i].rehash();
    }
    auto d = get_data();
    if (d.data != nullptr) {
        bptr.checksum = hash_data(d.data, d.size);
        d.free();
    }
}

VirtualBlkPtr *
VirtualBlkPtr::search(size_t offset) 
{
    if (bptr.levels <= 1) {
        return this;
    }
    initList();
    for (int i = 0; i < refs.nptrs; i++) {
        auto &p = refs.ptrs[i];
        if (p.is_empty()) return &p;
        bool lower = offset >= p.bptr.offset;
        bool upper = offset <= p.bptr.offset + p.bptr.size;
        if (lower && upper) {

            return p.search(offset);
        }
    }

    // We return the next pointer due to the fact this means this ptr is full.  We chose
    // this parent due to the reason the next pointer was completely empty.  If we
    // go through the full list and still cant find it, it must be the very next pointer.
    return refs.ptrs[refs.nptrs - 1].next();
}

bool
VirtualBlkPtr::is_zeros()
{
    return bptr.addresses[0].vdev == 0 &&
        (blocks.bc_entry == nullptr || blocks.bc_entry == blocks.bc->tbz_block());
}
