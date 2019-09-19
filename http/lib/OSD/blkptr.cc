#include <sstream>

#include <celestis/hash.h>
#include <celestis/OSD/blkptr.h>
#include <celestis/OSD/util.h>

blkptr
blkptr_init_root(int id)
{
   blkptr ptr = { 
       .addresses = {init_root_dva(id),{},{}}, //The root blkptr is going to point to the MOS object set
       .hash_type = BLKPTR_HASHTYPE_SKEIN256,
       .checksum = Hash {
           .hash = {},
       },
       .levels = 1
   };
   return ptr;
}

blkptr
blkptr_null()
{
    blkptr blk = {
        .addresses = {dva_null(), {}, {}},
        .hash_type = BLKPTR_HASHTYPE_SKEIN256,
        .checksum = Hash {
            .hash = {},
        },
        .levels = 1,
	.birth_txg = 0,
        .size = 0,
        .offset = 0
    };
    return blk;
}

std::string
blkptr_to_str(const blkptr &b, int tab_spacing)
{
    std::stringstream ss;

    ss << " Blockpointer {" << std::endl;
    ss << "\t.dva[0] = vdev: " << b.addresses[0].vdev << ", asize: " << b.addresses[0].asize;
    ss << ", offset: "<< b.addresses[0].offset << std::endl;
    ss << "\t.checksum = " << hash_to_string(b.checksum) << std::endl;
    ss << "\t.hash_type = " << b.hash_type << std::endl;
    ss << " } ";

    return tab_prepend(ss, tab_spacing);
}
