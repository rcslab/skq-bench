/**
 * @file allocator.h
 * @defgroup Allocators
 * List of allocators that implement the pure virtual class Allocator.
 * @{
 *
 * @ingroup ObjectStoreDevice
 *
 * @class Allocator
 *
 * @brief A pure virtual class which allows us to modularize our allocator 
 * implementations..
 *
 * @author Kenneth R Hancock
 * 
 */
#ifndef __CELESTIS_FS_ALLOCATOR_H__
#define __CELESTIS_FS_ALLOCATOR_H__

#include <stdlib.h>
#include <sys/uio.h>

#include <vector>

#include "blkptr.h"
#include "../pstats.h"

struct AllocStats {
    size_t fspace;
    size_t space;
};

class Allocator {
    public:
        Allocator(){};

        ~Allocator() {};

        
        /**
         * alloc will try and allocate near the given blkptr p, default null 
         * blkptr means no constraint of its location on disk.
         *
         * @param len The size of data required.
         * @param p The blkptr to try and find space by.
         * @param num_of_copies The number of copies we want the allocator to 
         * find
         * @return A vector of blkptr's which point to empty space on disk.
         */
        virtual std::vector<blkptr> alloc(size_t len, blkptr p = blkptr_null(), int num_of_copies = 1)=0;


        /**
         * free will free the block pointed to by p to allow for future 
         * allocations.  This function may be paired with the expire function
         * as some deallocation requires a freeing of the block, followed by
         * an expiring of the block.
         *
         * @param p The blkptr to free
         */
        virtual void free(blkptr p)=0;

        /**
         * Expire will transition the space pointed to by block p to a fully 
         * free state, meaning the allocator can now use this block to 
         * reallocate new data.
         *
         * @param p The blkptr to expire
         */
        virtual void expire(blkptr p)=0;


        virtual AllocStats stats()=0;
        virtual void flush()=0;
        /**
         * The to_string function for the allocator
         * 
         * @return string representation of the state of the allocator
         */
        virtual std::string to_string(int tab_spacing = 0)=0;

};
#endif
// @}
