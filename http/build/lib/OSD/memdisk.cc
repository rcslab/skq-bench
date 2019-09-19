#include <sys/types.h>
#include <sys/mman.h>
#include <emmintrin.h>

#include <celestis/pstats.h>
#include <celestis/OSD/memdisk.h>

#define LINE_SIZE 64

struct CacheLine {
    char buffer[64];
};

STAT_TIMER(MEMW, "mem.write.singleton", PERF_UNITS_CYCLES);
STAT_TIMER(MEMR, "mem.read.singleton", PERF_UNITS_CYCLES);

void flush(char * buffer, size_t len) 
{
    auto add_one = len % LINE_SIZE;
    auto num = (len / LINE_SIZE);
    auto cache_lines = (CacheLine *)buffer;
    
    int i = 0;
    for (; i < num; i++) {
        _mm_clflush(&cache_lines[i]);
    }

    if (add_one) {
        _mm_clflush(&cache_lines[i + 1]);
    }
}

MemDisk::MemDisk(size_t size, uint32_t id) : size(size), id(id)
{
    asize = size - (4 * MEGABYTE) - (512 * KILOBYTE);
    memory = (char *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);
}

ssize_t 
MemDisk::read(char * buffer, size_t len, off_t offset) 
{
    STAT_TSAMPLE_START(MEMR);
    memcpy(buffer, memory + offset, len);
    STAT_TSAMPLE_STOP(MEMR);
    return len;
}

ssize_t 
MemDisk::read(SGArray &arr, off_t offset)
{
    auto len = 0;
    for (auto ex : arr.split()) {
        auto g = ex->con_iovec();
        auto start = offset + g->offset;
        auto section = 0;
        for (int i = 0; i < g->size; i++) {
            auto io = g->ios[i];
            memcpy(io.iov_base, memory + start, io.iov_len);
            start += io.iov_len;
            section += io.iov_len;
        }
        len += section;
    }
    return len;
}
ssize_t 
MemDisk::write(char *buffer, size_t len, off_t offset) 
{    
    STAT_TSAMPLE_START(MEMW);
    memcpy(memory + offset, buffer, len);
    flush(memory + offset, len);
    STAT_TSAMPLE_STOP(MEMW);
    return len;
}

ssize_t 
MemDisk::write(SGArray &arr, off_t offset) 
{
    auto len = 0;
    for (auto ex : arr.split()) {
        auto g = ex->con_iovec();
        auto start = offset + g->offset;
        auto section = 0;
        for (int i = 0; i < g->size; i++) {
            auto io = g->ios[i];
            memcpy(memory + start, io.iov_base, io.iov_len);
            start += io.iov_len;
            section += io.iov_len;
        }
        len += section;
        flush(memory + offset, section);
    }
    return len;
}

ssize_t 
MemDisk::pwritev(iovec *io, size_t iovcnt, off_t offset) 
{
    return 0;
}
size_t 
MemDisk::get_size() 
{
    return size;
}
size_t 
MemDisk::get_asize() 
{
    return size;
}
uint32_t 
MemDisk::get_id()
{
    return id;
}

