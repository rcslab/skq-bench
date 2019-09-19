#include <string>
#include <sstream>
#include <math.h>

#include <celestis/debug.h>
#include <celestis/hash.h>
#include <celestis/OSD/dinode.h>
#include <celestis/OSD/util.h>

std::string
dinode_to_string(const dinode &d, int tab_spacing)
{
    std::stringstream ss;

    ss << "DInode {" << std::endl;
    ss << "\t.type = " << d.type << std::endl;
    ss << "\t.blkptr = " << std::endl;
    for(int i = 0; i < INODE_MAX_BLKPTR; i++) {
        blkptr p = d.blkptr[i];
        ss << "\t[" << i << "] = " << std::endl; 
        ss << blkptr_to_str(p, tab_spacing + 1) << std::endl;
    }
    ss << "}" << std::endl;

    return tab_prepend(ss, tab_spacing);

}

Hash
dinode_hash(const dinode &d)
{
    return hash_data((void *)&d, sizeof(d));
}

bool
dinode_verify(dinode d, Hash h)
{
    if(d.type == INODETYPE_METANODE) {
        d.blkptr[0].checksum = Hash {
            .hash = {}
        };
    } 
    Hash to_verify = hash_data((void *)&d, sizeof(d));
    return hash_is_equal(to_verify, h);
}

size_t
dinode_size(const dinode &d, int log2_block_size)
{
    size_t sum = 0;
    for(int i = 0; i < INODE_MAX_BLKPTR; i++) {
        sum += align_to_block(d.blkptr[i].addresses[0].asize, (1 << log2_block_size));
    }
    return sum;
}
