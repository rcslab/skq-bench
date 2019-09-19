/**
 * @defgroup BuddyAllocator
 * An allocator using a simple bitmap organized using a tree class.
 * @{
 * @ingroup Allocators
 *
 * @class BuddyAllocator
 *
 * @brief A Buddy Allocator for Memory Pages
 *
 * @author Kenneth R Hancock
 */ 
#ifndef __CELESTIS_BUDDYALLOC_H__
#define __CELESTIS_BUDDYALLOC_H__

#include <vector>
#include <string>
#include <iostream>
#include <unordered_map>

#include "memalloc.h"
#include "OSD/calloc.h"
#include <sys/queue.h>

class BuddyAllocator : public MemoryAllocator
{
    public:
        BuddyAllocator(size_t size, int min, int cut, int maxPool);
        ~BuddyAllocator();
        void * alloc(size_t size, AllocType t = AllocType::DEFAULT);
        void free(void * buffer);
        void debugVerify();
        MemAllocStats stats();
        std::string toString(int tab_spacing = 0);
#ifdef CELESTIS_DEBUG
        std::unordered_map<char *, size_t> debug_map;
#endif

    private:
        enum EntryStatus {INVALID = 0, USED, FREE, END};

        struct Entry {
            uint8_t order;
            EntryStatus status;
            LIST_ENTRY(Entry) pointers;
        };

        LIST_HEAD(ehead, Entry);
        typedef std::vector<ehead> MemTable;

        size_t fspace;
        size_t space;
        size_t n;
        int MIN;
        int CUTOFF;
        int MAX_POOL; 
        void * region;
        MemTable memoryRegions;
        std::vector<size_t> sizeTable;
        Entry * entries;

        void * allocHelper(size_t size);
        int splitTo(uint8_t to_bit);
        bool isMergable(Entry * e, uint8_t log2_size);
        void tryMerge(Entry * ptr);
        void mergeEntries(Entry * pre, Entry * next);

        void * entryToRegion(Entry * ptr);
        Entry * regionToEntry(void * buffer);

        void markInvalid(Entry * start, size_t num_entries);
        void mark(Entry * ptr, uint8_t log2_size, EntryStatus m);

        size_t getIndex(Entry * ptr);
        Entry * getNext(Entry * start);
        Entry * getPrev(Entry * start);
        Entry * getEnd(Entry * start);
};
#endif
// @}
