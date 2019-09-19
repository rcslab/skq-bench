#include <string>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cerrno>

#include <celestis/debug.h>
#include <celestis/pstats.h>
#include <celestis/OSD/defines.h>
#include <celestis/OSD/filedisk.h>
#include <celestis/OSD/sgarray.h>

std::string  name = "disk_";
int num = 0;

STAT_TIMER(DISKWARR, "disk.write.arr", PERF_UNITS_CYCLES);
STAT_TIMER(DISKRARR, "disk.read.arr", PERF_UNITS_CYCLES);
STAT_TIMER(DISKW, "disk.write.singleton", PERF_UNITS_CYCLES);
STAT_TIMER(DISKR, "disk.read.singleton", PERF_UNITS_CYCLES);

// Please note that filedisks need to be at least 4.5 megabytes in size!
FileDisk::FileDisk(size_t size) : size(size)
{
    ASSERT(size >= 4.5 * MEGABYTE)
    asize = size - (4 * MEGABYTE) - (512 * KILOBYTE);

    std::string n = name + std::to_string(num) + ".img";
    num++;
    file = ::open(n.c_str(), O_CREAT | O_SYNC | O_DIRECT | O_RDWR, S_IRWXU);
    if (ftruncate(file, size) == 0) {
    	LOG("Disk created of file - %li", size);
    } else {
        perror("ftruncate:");
        exit(EXIT_FAILURE);
    }
}

ssize_t
FileDisk::write(char *buffer, size_t len, off_t offset)
{
    STAT_TSAMPLE_START(DISKW);
    auto written = ::pwrite(this->file, buffer, len, offset);
    if (written == len) {

        STAT_TSAMPLE_STOP(DISKW);
    	return written;
    }
    SYSERROR ("pwrite: %s", strerror(errno));
    STAT_TSAMPLE_STOP(DISKW);
    return -1;
}

ssize_t
FileDisk::pwritev(iovec *io, size_t size, off_t offset)
{

    STAT_TSAMPLE_START(DISKW);
    auto w = ::pwritev(this->file, io, size, offset);
    STAT_TSAMPLE_STOP(DISKW);
    return w;
}

ssize_t
FileDisk::read(char *buffer, size_t len, off_t offset)
{
    STAT_TSAMPLE_START(DISKR);
    unsigned long read = ::pread(this->file, buffer, len, offset);
    if (read == len) {
        STAT_TSAMPLE_STOP(DISKR);
        return read;
    }
    STAT_TSAMPLE_STOP(DISKR);
    return -1;
}

ssize_t
FileDisk::read(SGArray &arr, off_t offset)
{
    STAT_TSAMPLE_START(DISKRARR);
    for (auto ex : arr.split()) {
        auto g = ex->con_iovec();
        auto output = ::preadv(this->file, g->ios, g->size, g->offset + offset);
        if (output == -1) {
            DLOG("%lu", output);
            SYSERROR("%s", strerror(errno));
            PANIC();
        }
    }
    STAT_TSAMPLE_STOP(DISKRARR);
    return 0;
}

ssize_t
FileDisk::write(SGArray &arr, off_t offset)
{
    STAT_TSAMPLE_START(DISKWARR);
    for (auto ex : arr.split()) {
        auto g = ex->con_iovec();
        auto output = ::pwritev(this->file, g->ios, g->size, g->offset + offset);
        if (output == -1) {
            DLOG("%lu", output);
            SYSERROR("%s", strerror(errno));
            PANIC();
        }
    }
    STAT_TSAMPLE_STOP(DISKWARR);
    return 0;
}

size_t
FileDisk::get_size()
{

    return this->size;
}

size_t
FileDisk::get_asize()
{
    return this->asize;
}

uint32_t
FileDisk::get_id()
{
    return this->file;
}


