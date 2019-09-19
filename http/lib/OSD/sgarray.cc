#include <celestis/debug.h>
#include <celestis/OSD/sgarray.h>
#include <celestis/OSD/blockcache.h>
#include <celestis/OSD/vblkptr.h>

using namespace std;

void
SGArray::add_bentry(off_t offset, BEntry * e) 
{
    add(e->size, offset, e->buffer, e->get_free_func(), e);
};

SGEntry *
SGArray::get(const int index) 
{
    return &entries[index];
}

size_t
SGArray::size()
{
    return entries.size();
}

void
SGArray::add(blkptr ptr, VData d) 
{
    
        add(
            ptr.addresses[0].asize,
            ptr.addresses[0].offset, 
            d.data,
            d.free,
            nullptr
        );

}

void
SGArray::add(VirtualBlkPtr * ptr)
{
    add(ptr->bptr.size, ptr->bptr.offset, 
            ptr->blocks.bc_entry->buffer,ptr->blocks.bc_entry->get_free_func(), ptr);

}

sg_ios * 
SGSection::con_iovec() const
{
    if (!size) {
        return new sg_ios {
            .ios = nullptr,
            .size = 0,
            .offset = 0,
        };
    }
    iovec * ios = new iovec[size];
    for (size_t i = 0; i < size; i++) {
        auto e = s + i;
        ios[i] = iovec {
            .iov_base = e->buffer,
            .iov_len = e->len
        };
    }

    return new sg_ios {
        .ios = ios,
        .size = size,
        .offset = s->offset 
    };
}

void 
SGArray::callback()
{
    for(auto &&e: entries) {
        if (e.free != 0) {
            e.free();
        }
    }
}

std::vector<SGSection *> 
SGArray::split(const size_t iov_max)
{
    sort();
    std::vector<SGSection *> arrays;
    size_t count = 1;
    auto it = begin();
    SGSection * sec = new SGSection {
        .s = begin(),
        .e = end(),
        .size = 2
    };
    arrays.push_back(sec);

    while(it != end()) {
        it++;
        count++;
        auto offset = (size_t)it->offset != (size_t)(it - 1)->offset + (it - 1)->len ;
        if (count == iov_max || offset) {
            arrays.back()->e = it;
            arrays.back()->size = count - 1;
            arrays.push_back(new SGSection {
                .s = it,
                .e = end(),
                .size = 2
            });
            count = 1;
        }
        
    }
    arrays.back()->size = count - 1;

    return arrays;;
}

void 
SGArray::add(SGArray &array) 
{
    for(auto &e : array.entries) {
        entries.push_back(e);
    };
}

void 
SGArray::add(SGEntry e) 
{
    entries.push_back(e);
}


void 
SGSection::callback() const
{
    for(auto it = s; it != end(); it++) {
        if(it->free != 0) {
            it->free();
        }
    }
}
void 
SGArray::add(size_t len, off_t offset, 
        void * buffer, std::function<void()> free, void * handle) 
{
    entries.push_back(SGEntry {
        .len = len,
        .offset = offset,
        .buffer = buffer,
        .free = free,
        .handle = handle
    });
}

void 
SGArray::add(off_t offset, size_t len, void * buff)
{
    add(len, offset, buff, 0, nullptr);
};

void 
SGArray::sort() 
{
    auto comparator = [](SGEntry a, SGEntry b){
        return a.offset < b.offset;
    };  
    std::sort(entries.begin(), entries.end(), comparator);
}




