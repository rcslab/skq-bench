#include <unistd.h>
#include <sys/uio.h>
#include <string> 
#include <iostream>
#include <sstream>
#include <vector>

#include <celestis/debug.h>
#include <celestis/hash.h>
#include <celestis/OSD/storagepool.h>
#include <celestis/OSD/btreealloc.h>
#include <celestis/OSD/diskvnode.h>
#include <celestis/OSD/dinode.h>
#include <celestis/OSD/diskosd.h>

#define UB_SIZE_ARRAY (128)
#define NUMBER_OF_INODES (15)
#define OFFSET (4 * MEGABYTE)

using namespace std;
/*
 * Storagepool construct
 * Input: 
 * disks - list of disks that will be used within the allocator
 * num_of_disks - number of disks used
 * block_size - size of the filesystem blocksize, default is 4KB.
 *
 */
StoragePool::StoragePool(DiskOSD * os, vector<Disk *> disks, int log2_block_size, bool init) 
    : block_size(log2_block_size)
{
    LOG("Creating storagepool");
    bc = os->bc;
    for (auto &p : disks) {
        this->disks[p->get_id()] = p;
    }

    if (!init) {
        return;
    }

    StoragePool::init_disks(disks);
    auto bc_entry = bc->alloc(1 << log2_block_size);
    alloc_file = new VNode(os, bc_entry, VType::VREG, 1);
    init_allocator(alloc_file, disks);
    create_pool(alloc_file);
}

void
StoragePool::init_allocator(CVNode * file, vector<Disk *> &disks)
{
    alloc_file = file;
    alloc = new BTreeAlloc(disks, alloc_file, block_size);
}

/*
 * 
 * create_pool creates the initial metanode, and the first transaction group 
 * etc, and boot straps the disk pool for FIRST time usage.
 *
*/
void
StoragePool::create_pool(CVNode * alloc){

    size_t bs = (1 << block_size);
    ub_loc latest = this->get_latest_ub();
    // List of writes to make
    // MOS object write, and DNODES Writes
    // First we setup the MOS Object to point to its list of inodes, we need to 
    // tell the allocator that we need space which will give us the address to 
    // modify our MOS object
    dinode metad {
        .type = INODETYPE_METANODE,
    };
    
    blkptr ub_ptr = blkptr_null();
    blkptr allocator_ptr = blkptr_null();
    // Fill the Block with inodes
    ub_ptr.addresses[0] = this->alloc->alloc(bs)[0].addresses[0];
    ub_ptr.size = bs;
    ub_ptr.levels = 1;

    metad.numbp = INODE_MAX_BLKPTR;

    for (int i = 0; i < INODE_MAX_BLKPTR; i++){
        blkptr mos_ptr = blkptr_null();
        metad.blkptr[i] = mos_ptr;
    }
    metad.blkptr[1] = this->alloc->alloc(bs)[0];

    DLOG("Initialized all blkptrs for metanodes");

    // Now ask for the INODES so we lock those
    // Setup the Allocators on disk representation and make space for it
    // We have to allocate enough space first as this will set the tree in 
    // itself
    auto size = alloc->stats().size;
    allocator_ptr.addresses[0] = this->alloc->alloc(size)[0].addresses[0];
    allocator_ptr.size = allocator_ptr.addresses[0].asize;
    // Put this to byte
    // Hash allocator data
    //Set Node 1 to the allocator data
    dinode allocator_block  = {
        .blkptr[0] = allocator_ptr,
        .type = INODETYPE_BITMAP,
        .asize = size,
        .size = size,
        .numbp = 1,
    };

    void * buffer = malloc(bs);
    memset(buffer, 0, bs);
    memcpy(buffer, &allocator_block, sizeof(dinode));
    metad.blkptr[1].checksum = hash_data(buffer, bs);
    metad.blkptr[1].size = bs;
    metad.blkptr[1].offset = bs;

    int err = write((char *)buffer, bs, metad.blkptr[1]);
    if (err < 0) {
	PANIC();
    }

    metad.blkptr[0] = ub_ptr;
    metad.blkptr[0].offset = 0;
    metad.blkptr[0].size = bs;
    metad.asize = 2 * bs; // We just have the metanode and bitmap
    metad.size = 2 * bs; 
    memset(buffer, 0, bs);
    memcpy(buffer, &metad, sizeof(dinode));
    ub_ptr.checksum = hash_data(buffer, bs);

    err = write((char *)buffer, bs, ub_ptr);
    if (err < 0) {
	PANIC();
    }

    latest.ub.rootbp = ub_ptr;
    this->update_uberblock(latest);
    DLOG("Done creating storagepool");
}

void
StoragePool::init_disks(vector<Disk *> disks)
{
    for (auto &disk : disks) {
        StoragePool::init_disk(disk);
    }
}

int
StoragePool::read(char * dst, size_t len, blkptr ptr)
{
    dva address = ptr.addresses[0];
    size_t disk_id = address.vdev;
    Disk * disk = this->get_disk(disk_id); 
    size_t poff = address.offset + OFFSET;

    int code = disk->read(dst, len, poff);

    if (ptr.hash_type) {
        DLOG ("%s", blkptr_to_str(ptr).c_str());
        Hash new_hash = hash_data((void *)dst, len);
        if (!hash_is_equal(new_hash, ptr.checksum)) {
            code = -1;
        };
    }

    return code;
}

int
StoragePool::write(DiskMap &disksga)
{
    typedef std::future<void> ret;
    auto F = [this](int t, SGArray &arr){
        disks[t]->write(arr, OFFSET);
        arr.callback();
    };
    std::vector<ret> returns;
    for (auto &w : disksga) {
        returns.push_back(pool.enqueue(F, w.first, w.second));
    }

    for (auto &&r : returns) {
        r.wait();
    }

    return 0;
}
    
ssize_t
StoragePool::write(char *buffer, size_t disk, size_t len, off_t offset) 
{
    size_t poff = offset + OFFSET;

    return this->disks[disk]->write(buffer, len, poff);
}

int
StoragePool::read(DiskMap &disksga)
{
    typedef std::future<void> ret;
    std::vector<ret> returns;
    for (auto &w : disksga) {
        auto F = [this, w](SGArray arr){
	    disks[w.first]->read(arr, OFFSET);
	    arr.callback();
        };
	returns.push_back(pool.enqueue(F, w.second));
    }
    for (auto &&r : returns) {
	r.wait();
    }
    return 0;
}

ub_loc
StoragePool::get_latest_ub()
{
    off_t offset = 128 * KILOBYTE;
    SGArray arr;
    for (int i = 0; i < UB_SIZE_ARRAY ; i++) {
        void * buff = malloc(sizeof(uberblock));
        auto l = [buff]() {
            free(buff);
        };
        arr.add(sizeof(uberblock), offset, buff, l, nullptr);
        // Check for "maximumness"
                // Creep forward on disk and read next section.
        offset += KILOBYTE;
    }

    uberblock max_ub;
    uint64_t max = 0;
    off_t final_offset = -1;
    int id = -1;
    for(auto &&d : disks) {
        d.second->read(arr, 0);

        auto e = arr.get(0);
        max_ub = *((uberblock *)e->buffer);
        max = max_ub.txg;
        final_offset = e->offset;
        for(auto &&entry : arr) {
            uberblock temp = *((uberblock *)entry.buffer);
            if (temp.txg > max){
                max = temp.txg;
                max_ub = temp;
                final_offset = entry.offset;
                id = d.second->get_id();
            }
        }
        // Is it a fresh mount?
        if (max != 0) {
            ASSERT(uberblock_verify(max_ub, max_ub.checksum))
            break;
        }
    }
    return ub_loc {
        .offset = final_offset,
        .disk_num = id,
        .ub = max_ub
    };
}

Disk *
StoragePool::get_disk(uint32_t id)
{
    return this->disks[id];
}

ssize_t
StoragePool::write(char * buffer, size_t len, blkptr ptr)
{
    // Get the disk number we are writing to
    int disk_id = ptr.addresses[0].vdev;
    // Get the Physical offset have to add 2 blocks.
    size_t poff = ptr.addresses[0].offset + OFFSET;
    Disk * disk = this->get_disk(disk_id);
    return disk->write(buffer, len, poff);
}



// Where we write the VDEV labels to disk and the VDEV objects
void
StoragePool::init_disk(Disk *d)
{
    const int SECTIONS = 256; // 128 uberblocks + 1 prefix buffer
    char prefix_buffer[KILOBYTE];
    char null_buffer[KILOBYTE];
    char start_buffer[KILOBYTE];

    memset(prefix_buffer, 0, KILOBYTE);
    memset(null_buffer, 0, KILOBYTE);
    memset(start_buffer, 0, KILOBYTE);

    uberblock ub_null_block = uberblock_create_null();
    uberblock start_block = uberblock_create_start(d->get_id());

    // For now we will init VDEV sections to just have 128 kb's of blank space
    iovec writes[SECTIONS]; 
    prefix_buffer[0] = 's'; // Nice to have for hexdumps right now
    prefix_buffer[1] = 't';
    iovec pre_write {
        .iov_base = &prefix_buffer,
        .iov_len = KILOBYTE
    };

    for (int i = 0; i < UB_SIZE_ARRAY; i++){
        writes[i] = pre_write;
    };

    //Starting root uberblock
    iovec start_io {
        .iov_base = &start_buffer,
        .iov_len = KILOBYTE
    };
    memcpy(&start_buffer, &start_block, sizeof(uberblock));
    writes[UB_SIZE_ARRAY] = start_io;

    //Null Blocks -- Just empty blocks for now
    memcpy(&null_buffer, &ub_null_block, sizeof(uberblock));
    iovec ub_null {
        .iov_base = &null_buffer,
        .iov_len = KILOBYTE 
    };

    for (int i = UB_SIZE_ARRAY + 1; i < SECTIONS; i++) {
        writes[i] = ub_null;
    }

    // List of offsets of where I want my VDEV sections to be placed on disk
    size_t offsets[4] = {
        0, 
        256 * KILOBYTE, 
        d->get_size() - (512 * KILOBYTE),
        d->get_size() - (256 * KILOBYTE)
    };

    //Allocator handles Blocks to physical address spac?
    for (int i = 0; i < 4; i++ ) {
        d->pwritev(writes, SECTIONS, offsets[i]);
    }

    DLOG("Disk %d VDEV label written", d->get_id());
}

/*
 *
 * update_uberblock will take in some uberblock helper struct, and write this 
 * uberblock to all disks within the disk pool
 *
 * Input:
 *  ub_h - a helper struct that contains the uberblock and an offset, the 
 *  offset is which uberblock it needs to be written to
 *
 */
void
StoragePool::update_uberblock(ub_loc &ub_h)
{
    ub_h.ub.checksum = Hash {
        .hash = {}
    };
    ub_h.ub.checksum = hash_data((void *)&(ub_h.ub), sizeof(uberblock));
    char buffer[KILOBYTE];
    DLOG("Updating uberblock : %s" , uberblock_to_str(ub_h.ub).c_str());
    memcpy(&buffer, &(ub_h.ub), sizeof(uberblock));
    for (const auto &disk : disks) {
        disk.second->write(buffer, KILOBYTE, ub_h.offset);
    }
}

ub_loc
StoragePool::get_next_ub(ub_loc &ub_l)
{
    ub_loc uh;
    if (ub_l.offset == -1) {
         uh = this->get_latest_ub();
    } else {
         uh = ub_l;
    }
    // Are you the last element?
    // TODO: This still doesnt account for different VDEV sectiosn and 
    // different disks
    if (uh.offset == 255 * KILOBYTE) {
        return ub_loc {
            .offset = 0,
            .disk_num = 0,
            .ub = uberblock_create_null()
        };
    } 

    return ub_loc {
        .offset = uh.offset + KILOBYTE,
        .disk_num = 0,
        .ub = uberblock_create_null()
    };
}

size_t
StoragePool::space()
{
    return this->alloc->stats().space;
}

std::string
StoragePool::to_string(int tab_spacing)
{
    std::stringstream ss;
    ss << "Storage pool state:" << std::endl;
    ss << "\tTotal Space: " << this->alloc->stats().space << " blocks" << std::endl;
    ss << this->alloc->to_string(tab_spacing) << std::endl;

    return tab_prepend(ss, tab_spacing);
}

