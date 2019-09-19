#include <celestis/OSD/btreealloc.h>
#include <string>
#include <sstream>

extern PerfTimer STAT_NAME(ALLOC);
extern PerfTimer STAT_NAME(FREE);

BTreeAlloc::BTreeAlloc(std::vector<Disk *> &disks, CVNode * file, size_t log2_block_size)
{
    /* TODO: So future problem I see, we need to track where these offsets 
     * exist on each disk, I suggest the pair, but it would mean we have to 
     * implement different types for multiBTrees
     */
    trees = new MultiBTree<size_t, size_t>(file, disks.size() * 2, log2_block_size);
    space = 0;
    fspace = 0;
    for (int i = 0; i < disks.size(); i++) {
        auto d = disks[i];
        auto size = d->get_asize();
        uint16_t id = d->get_id();
        sub_tree entry {
            .off = trees->get(i * 2),
            .len = trees->get((i * 2) + 1),
            .space = size,
            .fspace = 0,
            .vdev = id,
        };

        space += size; 
        auto ref = entry.len->getClosest(0);
        while(ref.found()) {
            fspace += ref.key(); 
            entry.fspace += ref.key(); 
            auto next = ref.next(); 
            ref = next;
        }

        if (!entry.fspace) {
            entry.len->insert(size, 0);
            entry.off->insert(0, size);
            entry.fspace = size;
        }
        fspace += entry.fspace;
        st[id] = entry;
    }
}

BTreeAlloc::sub_tree *
BTreeAlloc::get_min()
{
    size_t min_val = 0;
    sub_tree * min = &st.begin()->second;
    for (auto &e : st) {
        if (e.second.fspace > min_val) {
            min = &e.second;
            min_val = e.second.fspace;
        }
    }

    return min;
}

void
BTreeAlloc::flush()
{
    trees->flush();
}

std::vector<blkptr>
BTreeAlloc::alloc(size_t len, blkptr p, int num_of_copies)
{
    STAT_TSAMPLE_START(ALLOC);
    ASSERT(len > 0);
    auto sub = get_min();

    auto len_tree = sub->len;
    auto off_tree = sub->off;
    auto ref = len_tree->getClosest(len);
    auto key = ref.key();
    auto val = ref.val();
    len_tree->remove(ref);
    auto ref_off = off_tree->get(val);
    off_tree->remove(ref_off);
    // Key was not exact so have to add new space into tree
    blkptr ptr = blkptr_null();
    ptr.addresses[0].vdev = sub->vdev;
    ptr.addresses[0].asize = len;
    ptr.addresses[0].offset = val;

    if (key != len) {
        val += len;
        key -= len;
        len_tree->insert(key, val);
        auto s = key;
        key = val;
        off_tree->insert(key, s);
    }
    STAT_TSAMPLE_STOP(ALLOC);
    fspace -= len;
    sub->fspace -= len;
    return std::vector<blkptr> { ptr };
}

void
BTreeAlloc::free(blkptr p)
{
}

void 
BTreeAlloc::expire(blkptr p)
{
    STAT_TSAMPLE_START(FREE);
    auto offset = p.addresses[0].offset;
    auto size = p.addresses[0].asize;
    uint16_t id = p.addresses[0].vdev;
    auto sub = st[id];
    fspace += size;
    sub.fspace += size;
    sub.len->insert(size, offset);
    sub.off->insert(offset, size);
    STAT_TSAMPLE_STOP(FREE);
}

AllocStats
BTreeAlloc::stats()
{
    return AllocStats {
        .fspace = fspace,
        .space = space
    };
}

std::string
BTreeAlloc::to_string(int tab_spacing)
{
    std::stringstream ss;    
    ss << std::endl;
    for (auto &&e : st) {
        ss << "Offset Tree for Disk " << e.second.vdev;
        ss <<e.second.off->toString();
        ss << "Len Tree for Disk " << e.second.vdev;
        ss <<e.second.len->toString();
    }
    return ss.str();
}


