#include <string>
#include <iostream>

#include <celestis/debug.h>
#include <celestis/OSD/util.h>
#include <celestis/OSD/diskalloc.h>

using namespace std;
// Simplest form of an allocator right now.  Will just represent the disk as a 
// sequences of 1's and 0's. Where 1's show a block being used, 0's show a 
// block not in use.
DiskAllocator::DiskAllocator(Disk * disk, vector<BEntry *> entries, int log2_block_size)
    : disk(disk), entries(entries), log2_block_size(log2_block_size)
{
    size_t bs =  (1 << log2_block_size);
    this->disk_space = (disk->get_asize() / bs);
    this->free_disk_space = this->disk_space;
    for (auto &e : entries) {
        for (int i = 0; i < e->size; i++) {
            if (e->buffer[i] == '1') {
                this->free_disk_space--;
            } 
            if (e->buffer[i] == 'X') {
                e->buffer[i] = '0';
            }
        }
    }
}

dva
DiskAllocator::alloc_once(size_t len)
{
    size_t blocks = (len / (1 << log2_block_size));
    if (len % (1 << log2_block_size) != 0) {
            blocks++;
    } 
    size_t current_offset = 0;
    bool space_found = false;
    uint32_t size_of_space = 0;
    size_t offset = 0;
    for (auto &e : entries) {
        for (size_t i = 0; i < e->size; i++) {
            if (e->buffer[i] == '0') {
                if (!space_found) {
                    space_found = true;
                    offset = current_offset;
                }
                e->buffer[i] = '1';
                size_of_space++;
            } else {
                if (space_found) {
                    break;
                }
            }
            current_offset++;
            if (size_of_space == blocks) {
                break;
            }
        }
        if (space_found) {
            break;
        }
    }
    this->free_disk_space -= size_of_space;
    return dva {
        .vdev = this->disk->get_id(),
        .asize = size_of_space * (1 << log2_block_size),
        .offset = offset * (1 << log2_block_size)
    };
}

std::vector<blkptr>
DiskAllocator::alloc(size_t len, blkptr p = blkptr_null(), int num_of_copies = 1)
{

    if (len > (stats().fspace * (1 << log2_block_size))) {
        PANIC();
    }
    std::vector<blkptr> ptrs;
    while(len) {
        blkptr p = blkptr_null();
        p.addresses[0] = alloc_once(len);
        if (p.addresses[0].asize == 0 ) {
            PANIC();
        }
        if(p.addresses[0].asize > len) {
            p.addresses[0].asize = len;
            len = 0;
        } else {
            len -= p.addresses[0].asize;
        }
        ptrs.push_back(p);
    }
    if (ptrs.size() > 1) {
        PANIC();
    }
    return ptrs; 
}

void
DiskAllocator::set(blkptr p, char c)
{

    size_t bs = (1 << this->log2_block_size);
    int entry = p.addresses[0].offset / (bs * bs);
    int offset = (p.addresses[0].offset / bs) % bs;
    int blocks = align_to_block(p.addresses[0].asize, bs) / bs;
    for (int i = entry; i < this->entries.size(); i++ ) {
        BEntry * current = this->entries[i];
        for (int k = offset; k < current->size; k++) {
            if (blocks) {
                current->buffer[k] = c;
                blocks--;
                if (c == '0') {
                    this->free_disk_space++;
                }
            } else {
                break;
            }
        }
        if (!blocks) {
            break;
        }
        offset = 0;
    }

}
void
DiskAllocator::free(blkptr p)
{
    this->set(p, 'X');
}

void
DiskAllocator::expire(blkptr p)
{
    this->set(p, '0');
}


std::string
DiskAllocator::to_string(int tab_spacing)
{
    std::stringstream ss;
    ss << "Disk " << this->disk->get_id() << ": " << stats().fspace << " free space";
    ss << std::endl;

    return tab_prepend(ss, tab_spacing);
}


AllocStats
DiskAllocator::stats()
{
    return AllocStats {
        .fspace = free_disk_space,
        .space = disk_space,
    };
}
