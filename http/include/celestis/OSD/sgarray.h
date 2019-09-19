#ifndef __CELESTIS_FS_SGARRAY_H__
#define __CELESTIS_FS_SGARRAY_H__

#include <sys/types.h>
#include <sys/limits.h>
#include <sys/uio.h>

#include <algorithm>
#include <vector>
#include <unordered_map>


#include "util.h"


struct BEntry;
struct VData;
struct blkptr;
class VirtualBlkPtr;

struct SGEntry { 
    void * buffer; 
    size_t len;
    off_t offset;
    std::function<void()> free;
    void * handle;
    int status;
};

typedef std::vector<SGEntry>::iterator sgiterator;
typedef std::pair<off_t, size_t> rtuple;

struct sg_ios {
    iovec * ios;
    size_t size;
    off_t offset;
    ~sg_ios() {
        delete ios;
    }
};


struct SGSection {
    sgiterator s;
    sgiterator e;
    size_t size;

    sgiterator begin() const
    {
	return s;
    }

    sgiterator end() const
    {
	return e;
    }

    sg_ios * con_iovec() const;

    void callback() const;
};


class SGArray {
    private:
        std::vector<SGEntry> entries;
    public:
        SGArray() {};


        void add(SGArray &array);
        void add(size_t len, off_t offset, 
                void * buffer, 
                std::function<void()> free, 
                void * handle);
        void add(off_t offset, size_t len, void * buff);
        void add(blkptr ptr, VData d);
        void add(SGEntry e);
        void add_bentry(off_t offset, BEntry * e);
        void add(VirtualBlkPtr * ptr);
        void free() {
            for(auto &p : entries) {
                ::free(p.buffer);
            }
        }

        SGEntry * get(const int index);

        void write(void * buffer, off_t offset);
        void read(void * buffer, off_t offset);

        void sort();
        std::vector<SGSection *> split(const size_t iov_max = IOV_MAX);
        void callback();

        size_t size();

        void clear()
        {
            entries.clear();
        }

        SGEntry back()
        {
            return entries.back();
        }

        sgiterator begin() {
            return entries.begin();
        }
        sgiterator end() {
            return entries.end();
        }
};

#endif

