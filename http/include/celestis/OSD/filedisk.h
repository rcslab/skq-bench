#ifndef __CELESTIS_FS_FILEDISK_H__
#define __CELESTIS_FS_FILEDISK_H__
#include <string>

#include "disk.h"

class FileDisk : public Disk
{
    private:
        size_t asize;
        size_t size;
        int file;
    public:
        FileDisk(size_t size);
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
