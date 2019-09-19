#include <sys/types.h>

#include <stdint.h>
#include <stdint.h>
#include <stdio.h>
#include <iomanip>

#include <sstream>

#include <celestis/hash.h>
#include <celestis/OSD/uberblock.h>
#include <celestis/OSD/util.h>


uberblock
uberblock_create_null()
{
    uberblock null = {
        .magic = UB_MAGIC_NUM, 
        .txg = 0,
        .timestamp = 0, 
        .rootbp = blkptr_null()
    }; 

    return null;
}

uberblock
uberblock_create_start(int disk_id)
{
    //Start Uber block
    uberblock start = uberblock_create_null();
    start.txg = 1;
    start.timestamp = get_UTC();
    start.rootbp = blkptr_init_root(disk_id);
    start.checksum = Hash {
        .hash = {}
    };
    start.checksum = hash_data((void *)&start, sizeof(uberblock));

    return start;
}

std::string
uberblock_to_str(const uberblock &ub, int tab_spacing)
{
    std::stringstream ss;   

    ss << "Uberblock { " << std::endl;
    ss << "\t.magic = " << ub.magic << std::endl;
    ss << "\t.txg = " << ub.txg << std::endl;
    ss << "\t.timestamp = " << ub.timestamp << std::endl;
    ss << "\t.rootbp = " << std::endl;
    ss << "\t.checksum = " << hash_to_string(ub.checksum) << std:: endl;
    ss << blkptr_to_str(ub.rootbp, tab_spacing + 1) << std::endl;
    ss << "}" << std::endl;

    return tab_prepend(ss, tab_spacing);
}

bool
uberblock_verify(uberblock ub, Hash h)
{
    ub.checksum = Hash {
        .hash = {}
    };

    Hash check = hash_data((void *)&ub, sizeof(uberblock));

    return hash_is_equal(check, h);
}


