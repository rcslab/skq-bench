#include <string>
#include <sstream> 

#include <string.h>
#include <sys/mman.h>

#include <celestis/debug.h>
#include <celestis/pstats.h>
#include <celestis/buddyalloc.h>


#define MAX (40)

#define INSERT(bit, _entry) \
    LIST_INSERT_HEAD(&memoryRegions[bit],_entry, pointers); \
    sizeTable[bit]++;

#define REMOVE(bit, _entry) \
    LIST_REMOVE(_entry, pointers); \
    sizeTable[bit]--;

STAT_TIMER(BUDDYALLOC, "buddy.alloc", PERF_UNITS_CYCLES);
STAT_TIMER(BUDDYFREE, "buddy.free", PERF_UNITS_CYCLES);

BuddyAllocator::~BuddyAllocator() 
{
    munmap(region, space);
    delete entries;
}

BuddyAllocator::BuddyAllocator(size_t size, int min, int cut, int maxPool) :
    MIN(min), CUTOFF(cut), MAX_POOL(maxPool)
{
    for (int i = 0; i <= MAX; i++) {
        memoryRegions.push_back(ehead {});
        LIST_INIT(&memoryRegions.back());
        sizeTable.push_back(0);
    }
    auto bit = fls(size) - 1;
    fspace = 1 << bit;
    space = fspace;
    region = mmap(0, fspace, PROT_READ | PROT_WRITE, 
            MAP_ANON, -1, 0);

    mlock(region, fspace);

    if (region == MAP_FAILED) {
        perror("ERROR");
        PANIC();
    }

    n = 1 << (bit - MIN);
    entries = new Entry[n];
    entries[0] = Entry {
        .order = static_cast<uint8_t>(bit),
        .status = FREE,
    };
    markInvalid(&entries[1], n - 2);
    entries[n - 1] = Entry {
        .order = static_cast<uint8_t>(bit),
        .status = END
    };

    INSERT(bit, &entries[0]);
}

BuddyAllocator::Entry *
BuddyAllocator::regionToEntry(void * buff)
{
    size_t i = ((char *)buff - (char *)region) >> MIN;
    return &entries[i];
}

void * 
BuddyAllocator::entryToRegion(Entry * ptr)
{
    return (void *)((uintptr_t)region + (getIndex(ptr) << MIN));
}

size_t 
BuddyAllocator::getIndex(Entry * ptr)
{
    uintptr_t base = ((uintptr_t)ptr - (uintptr_t)entries);

    return base / sizeof(Entry);
}

void
BuddyAllocator::markInvalid(Entry * ptr, size_t num_entries)
{
    memset(ptr, 0, num_entries * sizeof(Entry));
}

void
BuddyAllocator::mark(Entry * ptr, uint8_t log2_size, EntryStatus m)
{
    ptr->order = log2_size;
    ptr->status = m;
    auto size = 1 << (log2_size - MIN);
    if (size > 1) {
        markInvalid(&ptr[2], size - 2); 
        ptr[size - 1].status = END;
        ptr[size - 1].order = log2_size;
    }
}

void *
BuddyAllocator::alloc(size_t size, AllocType t) 
{
    STAT_TSAMPLE_START(BUDDYALLOC);
    auto buff = allocHelper(size);
    STAT_TSAMPLE_STOP(BUDDYALLOC);
#ifdef CELESTIS_DEBUG
    auto largest = flsl(size);
    largest = (1 << (largest - 1)) == size ? largest - 1 : largest;
    largest = MIN > largest ? MIN : largest;
    auto entry = regionToEntry(buff);
    ASSERT(entry->order == largest);
    auto actual_size = 1 << largest;
    for (auto &e : debug_map) {
        auto lower = e.first;
        auto upper = lower + e.second - 1;
        auto check = (char *)buff + actual_size - 1;
        if (buff >= lower && buff <= upper) {
            PANIC();
        }
        if (check >= lower && check <= upper) {
            PANIC();
        }
        if (buff <= lower && check >= upper) {
            PANIC();
        }
        if (lower <= buff && upper >= check) 
        {
            PANIC();
        }
    }
    debug_map[(char *)buff] = actual_size;
#endif
    return buff;
}

void *
BuddyAllocator::allocHelper(size_t size)
{
    auto largest = flsl(size);
    largest = (1 << (largest - 1)) == size ? largest - 1 : largest;
    largest = MIN > largest ? MIN : largest;
    ASSERT(fspace >= (1 << largest));
    if (!LIST_EMPTY(&memoryRegions[largest])) {
        auto entry = LIST_FIRST(&memoryRegions[largest]);
        REMOVE(largest, entry);
        mark(entry, largest, USED);
        auto address = entryToRegion(entry);
        fspace -= 1 << largest;

        return address;
    } else {
        if (splitTo(largest) < 0) {
            //TODO mergeTo needed. If we have the free space that means we 
            // must have enough space so force the merge.
            PANIC();
        }

        return this->allocHelper(size);
    }

}

BuddyAllocator::Entry *
BuddyAllocator::getEnd(Entry * ptr)
{
    size_t i = getIndex(ptr);
    auto size = 1 << (ptr->order - MIN);
    return &entries[i + size - 1];
}

void
BuddyAllocator::mergeEntries(Entry * pre, Entry * next)
{
    auto end = getEnd(next);
    next->status = INVALID;
    next->order = INVALID;
    end->order = pre->order + 1;
    pre->status = FREE;
    pre->order = pre->order + 1;
    next->order = pre->order;
    REMOVE(pre->order, pre);
    REMOVE(next->order, next);
    INSERT(pre->order, pre);
}

bool
BuddyAllocator::isMergable(Entry * entry, uint8_t log2_size) 
{
    return entry != nullptr && entry->status == FREE && entry->order == log2_size;
}

void
BuddyAllocator::tryMerge(Entry * entry)
{
    Entry * pre = entry;
    Entry * next = entry;
    auto check = 1 << entry->order & (uintptr_t)entry;
    if (check) {
        pre = getPrev(entry);
    } else {
        next = getNext(entry);
    }

    if (isMergable(pre, entry->order) && isMergable(next, entry->order)) {
        mergeEntries(pre, next);
        if (sizeTable[pre->order] >= MAX_POOL) {

            return tryMerge(pre);
        }
    }
}

void
BuddyAllocator::free(void * buffer)
{
    STAT_TSAMPLE_START(BUDDYFREE);
    auto entry = regionToEntry(buffer);
#ifdef CELESTIS_DEBUG
    auto it = debug_map.find((char *)buffer);
    ASSERT(it != debug_map.end());
    debug_map.erase(it);
#endif
    ASSERT(entry->status == USED);
    mark(entry, entry->order, FREE);
    fspace += 1 << entry->order;
    if (entry->order > CUTOFF) {
        tryMerge(entry);
    } else {
        INSERT(entry->order, entry);
    }

    STAT_TSAMPLE_STOP(BUDDYFREE);
}

BuddyAllocator::Entry *
BuddyAllocator::getPrev(Entry * ptr)
{
    size_t i = getIndex(ptr);
    if (i == 0) {
    
        return nullptr;
    }
    auto e = &entries[i - 1];
    auto size = 1 << (e->order - MIN);
    return &entries[i - size];
}

BuddyAllocator::Entry *
BuddyAllocator::getNext(Entry * ptr)
{
    return getEnd(ptr) + 1;
}

int
BuddyAllocator::splitTo(uint8_t to_bit)
{
    if (to_bit > MAX) {

        return -1;
    }

    auto check = to_bit + 1;
    if (LIST_EMPTY(&memoryRegions[check])) {
        if (splitTo(check) < 0) {

            return -1;
        }
    }
    auto entry = LIST_FIRST(&memoryRegions[check]);
    ASSERT(entry->status == FREE);
    REMOVE(entry->order, entry);
    mark(entry, to_bit, FREE);
    auto entry_two = getNext(entry);
    mark(entry_two, to_bit, FREE);
    ASSERT(entry_two->status == FREE);
    INSERT(to_bit, entry_two);
    INSERT(to_bit, entry);

    return 0;
}

std::string
BuddyAllocator::toString(int tab_spacing)
{

    std::stringstream ss;
    for (int i = 0; i <= MAX; i++) {
        int k = 0;
        Entry * e;
        LIST_FOREACH(e, &memoryRegions[i], pointers) {
            k++;
        }
        ss << i << " - " << k << std::endl;
    }
    return ss.str();
}

void
BuddyAllocator::debugVerify()
{
    size_t s = 0;
    for (int i = 0; i <= MAX; i++) {
        Entry * e;
        LIST_FOREACH(e, &memoryRegions[i], pointers) {
            ASSERT(e->status == FREE);
            ASSERT(e->order == i);
            s += (1 << e->order);
        }
    }
    ASSERT(s == fspace);
}

MemAllocStats
BuddyAllocator::stats()
{
    return MemAllocStats {
        .fspace = fspace,
        .space = space
    };
}
