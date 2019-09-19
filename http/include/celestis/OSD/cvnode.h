#ifndef __CELESTIS_FS_CVNODE_H__
#define __CELESTIS_FS_CVNODE_H__
#include <stdint.h>
#include <string>
#include <functional>
#include <condition_variable>

#include "sgarray.h"

struct BEntry;

typedef uint64_t CNode;

struct CVNodeStat {
    CNode  inum;
    size_t size;
    size_t asize;
};

class CVNode {

    public:
        CVNode() {};
        virtual ~CVNode(){};

        virtual int write(SGArray &sga, std::function<void()> cb)=0; // Async
        int write(SGArray &sga) 
        {
            std::mutex lock;
            std::condition_variable cv;

            std::unique_lock<std::mutex> lk(lock);

            auto wait_func = [&cv]() 
            {
                cv.notify_one();
            };
            write(sga, wait_func);

            return 0;
        }

        virtual int read(SGArray &sga, std::function<void()> cb)=0; // Async
        int read(SGArray &sga)
        {
            std::mutex lock;

            std::condition_variable cv;
            auto wait_func = [&cv]() 
            {
                cv.notify_one();
            };

            std::unique_lock<std::mutex> lk(lock);

            read(sga, wait_func);

            return 0;
        }

        virtual void truncate(size_t bytes)=0;
        virtual void close()=0;
        virtual void retain()=0;
        virtual void release(size_t index)=0;
        virtual uint64_t _ref()=0;
        virtual CVNodeStat stats()=0;
        virtual size_t getSize()=0;
        virtual std::string to_string(int tab_prepend = 0)=0;
};

#endif
