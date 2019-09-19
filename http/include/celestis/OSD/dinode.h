/**
 * @ingroup OnDiskStructure
 * @{
 *
 * @author Kenneth R Hancock
 *
 */
#ifndef __CELESTIS_FS_DINODE_H__
#define __CELESTIS_FS_DINODE_H__

#include <stdint.h>
#include <sys/uio.h>

#include "blkptr.h"
#include "dva.h"

#define INODE_MAX_BLKPTR 15 // 12 Data pointer, 1 indirect, 1 double indirect, 1 triple indirect
#define FULL_BLOCK_SIZE 12

/**
 * @ingroup OnDiskStructure
 * InodeType is the type of inodes that current dinode is.
 */
enum InodeType : uint8_t {
    INODETYPE_FILE = 1,
    INODETYPE_METANODE = 2,
    INODETYPE_DIR = 3,
    INODETYPE_BITMAP = 4,
};

/**
 * @ingroup OnDiskStructure
 * @struct dinode
 *
 * @brief On disk representation of the filesystem inodes.
 *
 * The on disk representation of inodes for the filesystem ("D"isk inodes), 
 * inodes are the basic building blocks of our file system. Like most
 * This structure acts as a way to point directly to data or point to lists of 
 * more inodes (through indirect inodes).
 *
 */
struct dinode {
    InodeType type;  ///< Type of dinode
    uint8_t indirect; ///< Size of the indirect block pointers
    uint8_t numbp;  ///< Number of block pointers
    uint16_t blocksize; ///< Block size
    size_t asize; // Size allocated for inode
    size_t size; // Actual size asked for by user
    blkptr blkptr[INODE_MAX_BLKPTR];///< List of block pointers
};

#define INODE_ON_DISK_SIZE 4096
static_assert(INODE_ON_DISK_SIZE >= sizeof(dinode), "Wrong size of INODE_ON_DISK_SIZE");

/**
 * Initializes the metablock
 *
 * @param id The id in which the metablock is located (the vdev), the disk id.
 */
iovec * create_init_meta(int id);

/**
 * To string function for the dionde struct
 *
 * @param d The struct to stringify
 * @param tab_spacing The amount of tabs to prepend to this string
 *
 */
std::string dinode_to_string(const dinode &d, int tab_spacing = 0);

/**
 * Hash the dinode to some checksum
 */
Hash dinode_hash(const dinode &d);

/**
 * Verify some dinode d to some hash h
 */
bool dinode_verify(dinode d, Hash h);

/**
 * Helper function to calulate the overall size stored at the dinode, this will
 * go through each of the blkptrs and sum up their sizes.
 */
size_t dinode_size(const dinode &d, int log2_block_size = 12);

#endif
// @}

