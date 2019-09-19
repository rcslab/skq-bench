/**
 * @defgroup Allocators
 * List of allocators that implement the pure virtual class Allocator.
 * @{
 *
 * @ingroup ObjectStoreDevice
 *
 * @class MemoryAllocator 
 *
 * @brief A pure virtual class which allows us to modularize our allocator 
 * implementations..
 *
 * @author Kenneth R Hancock
 * 
 */
#ifndef __CELESTIS_FS_MEMALLOC_H__
#define __CELESTIS_FS_MEMALLOC_H__

struct MemAllocStats {
    size_t fspace;
    size_t space;
};

enum class AllocType {
    DEFAULT,
    RESERVED,
};

class MemoryAllocator {
    public:
        MemoryAllocator(){};

        ~MemoryAllocator() {};

        virtual void * alloc(size_t size, AllocType t = AllocType::DEFAULT)=0;
        virtual void free(void * buff)=0;
        virtual MemAllocStats stats()=0;
        virtual std::string toString(int tab_spacing = 0)=0;
};
#endif
// @}
