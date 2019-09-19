#ifndef __CELESTIS_FS_MEMDISK_H__
#define __CELESTIS_FS_MEMDISK_H__
#include <string>
#include "disk.h"

class MemDisk : public Disk
{
    private:
        size_t asize;
        size_t size;
        uint32_t id;
        char * memory;
    public:
        MemDisk(size_t size, uint32_t id);
        ssize_t read(char *buffer, size_t len, off_t offset);
        ssize_t read(SGArray &arr, off_t offset);
        ssize_t write(char *buffer, size_t len, off_t offset);
        ssize_t write(SGArray &arr, off_t offset);
        ssize_t pwritev(iovec *io, size_t iovcnt, off_t offset);
        size_t get_size();
        size_t get_asize();
        uint32_t get_id();

};

#endif
