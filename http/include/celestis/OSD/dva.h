/**
 * @ingroup OnDiskStructure
 * @{
 *
 * @author Kenneth R Hancock
 *
 */
#ifndef __CELESTIS_FS_DVA_H__
#define __CELESTIS_FS_DVA_H__

#include <stdint.h>

/**
 * @ingroup OnDiskStructure
 * 
 * DVA is the "Data Virtual Address", which is a way of addressing data on many 
 * disks.
 */
typedef struct dva {
    uint32_t vdev; ///< ID of the disk
    uint32_t asize; ///< Actual size of the data pointed to by the DVA
    uint64_t offset; ///< Offset data is at on the particular vdev disk
} dva;

/**
 * Create the init root dva - simple helper function
 *
 * @param id ID of the disk
 */
dva
init_root_dva(uint8_t id);

inline static dva
dva_null() {
    return dva {
        .vdev = 0,
        .asize = 0,
        .offset = 0 
    };
};

#endif
// @}
