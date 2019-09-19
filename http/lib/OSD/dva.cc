#include <celestis/OSD/dva.h>

dva
init_root_dva(uint8_t id)
{
    dva d = {
        .vdev = id,
        .offset = 0
    };

    return d;
}
