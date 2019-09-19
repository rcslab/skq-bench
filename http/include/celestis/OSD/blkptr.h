/**
 * @defgroup OnDiskStructure
 * Group of on disk representations of data structures
 * @{
 *
 * @author Kenneth R Hancock
 */

#ifndef __CELESTIS_FS_BLKPTR_H__
#define __CELESTIS_FS_BLKPTR_H__

#include <string>

#include "../hash.h"
#include "dva.h"

#define BLKPTR_NUM_DVA 3
#define SECTOR_SIZE 9

/**
 * hash_type is the type of hashing algorithm used on the block this block 
 * pointer is pointing to
 */
enum hash_type: uint8_t {
    BLKPTR_HASHTYPE_NONE = 0, ///< No hash used
    BLKPTR_HASHTYPE_SKEIN256 = 1 ////< Skein Hash used
};

/**
 * @struct blkptr
 * @brief On disk representation of blkptr
 * 
 * Blkptr is a struct used as a pointer to blocks on disk using dva addresses.
 */
struct blkptr {
    dva         addresses[BLKPTR_NUM_DVA]; ///< Addresses at which this data is stored
    size_t      size;
    size_t      offset;
    Hash        checksum; ///< 256 bit hash of the data
    hash_type   hash_type; ///< Hash algorithm used
    uint8_t     levels; ///< Current level of the blkptr,  blkptrs of greater than 1 would its indirect.
    uint64_t    birth_txg;
};

/**
 * blkptr_init_root inits the first blkptr used when bootstrapping the 
 * filesystem
 * @param id 
 */
blkptr blkptr_init_root(int id);

/**
 * blkptr_null creates a null blkptr.
 */
blkptr blkptr_null();

/**
 * blkptr_to_str creates a string of the current state of the blkptr.
 *
 * @param b blkptr to stringify
 * @param tab_spacing A variable used to prepend a certain level of tabs into 
 * the string version of the blkptr. 
 */
std::string blkptr_to_str(const blkptr &b, int tab_spacing = 0);
#endif
// @}
