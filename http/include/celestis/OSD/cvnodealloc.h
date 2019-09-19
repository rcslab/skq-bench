/**
 * @defgroup BlockAlloc
 * An allocator using a simple bitmap organized using a tree class.
 * @{
 * @ingroup Allocators
 *
 * @class BlockAlloc
 *
 * @brief This will create a block allocator given some file. Very simple 
 * implentation with standard free list
 *
 * When using this class the buffers that are returned with alloc call must 
 * preserve the blockheaders stored in each buffer.  We want to preserve these 
 * ids but also allow the user to have access to them
 *
 * @author Kenneth R Hancock
 */
#ifndef __CELESTIS_FS_CVNODEALLOC_H__
#define __CELESTIS_FS_CVNODEALLOC_H__

#include <celestis/debug.h>
#include <celestis/pstats.h>
#include <celestis/OSD/cvnode.h>


struct blockheader 
{
    size_t id;
};

class BlockAlloc 
{
    private:
        struct allocheader {
            size_t n;
        };
        int * freelist;
        CVNode * file;
        allocheader * h;
        void * buffer;
        size_t max_n; 
        std::unordered_map<size_t, void *> map;
    public:
        size_t bs;

        BlockAlloc(){};
        BlockAlloc(CVNode * file, size_t log2_block_size)
        {
            init(file, log2_block_size);
        };

        ~BlockAlloc()
        {
            delete h;
        }


        void init(CVNode * file, size_t log2_block_size)
        {
            this->file = file;
            ASSERT(log2_block_size > 6);
            bs = 1 << log2_block_size;

            // Brand new
            auto stats = file->stats();
            if(!stats.size) {
                file->truncate(bs);
            }
            auto arr = SGArray();
            buffer = malloc(bs);
            bzero(buffer, bs);
            arr.add(0, bs, buffer);
            file->read(arr);
            h = (allocheader *)buffer;
            freelist = (int *)((char *)buffer + sizeof(allocheader));
            max_n = (bs - sizeof(allocheader)) / sizeof(int);
            map[0] = buffer;
        }

        void *alloc() 
        {
            int block;
            if (!h->n) {
                auto current_size = file->stats().size;
                auto size = current_size + (bs);
                file->truncate(size);
                block = (current_size / bs);
            }  else {
                block = freelist[--h->n];
            }

            SGArray blocks = SGArray();
            blocks.add(block * bs, bs, malloc(bs));
            file->read(blocks);
            auto buff = blocks.get(0)->buffer;
            auto td = (blockheader *)buff;
            td->id = block;
            map[td->id] = buff; 
            return buff;
        }

        std::vector<void *> getv(std::vector<size_t> &multi_id)
        {
            std::vector<void *> buffers;
            auto arr = SGArray();
            for (auto &&id : multi_id) {
                auto buff = cache(id);
                if (buff == nullptr) {
                    arr.add(id * bs, bs, malloc(bs));
                }
            }
            file->read(arr);
            for (size_t i = 0; i < multi_id.size(); i++) {
                ASSERT((((blockheader *)arr.get(i)->buffer))->id == multi_id[i]);
                buffers.push_back(arr.get(i)->buffer);
            }
            return buffers;
        }

        void *get(size_t id)
        {
            void * buff = cache(id);
            if (buff == nullptr) {
                auto arr = SGArray();
                arr.add(id * bs, bs, malloc(bs));
                file->read(arr);
                buff = arr.get(0)->buffer;
                map[id] = buff;
            } 
            ASSERT(((blockheader *)buff)->id == id);
            return buff;

        }
        void flush()
        {
            SGArray writes = SGArray();
            for (auto &e : map) {
                writes.add(e.first * bs, bs, e.second);
            }
            file->write(writes);
        }

        void *cache(size_t id)
        {
            if (map.find(id) == map.end()) { 
                return nullptr;
            }
            return map[id];
        }

        void free(void * block) 
        {
            size_t id = *((size_t *)block);
            bzero(block, bs);
            ((blockheader *)block)->id = id;
            freelist[h->n++] = id;
        }
};
#endif
