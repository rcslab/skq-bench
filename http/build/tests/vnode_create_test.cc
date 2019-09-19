#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <string.h>
#include <sys/types.h>

#include <celestis/testing/osdhelpers.h>

#include "test.h"

using namespace std; 

#define MEGABYTES (1024 * 1000)
#define NUM_INODES (10)
#define BLOCK_SIZE (1 << 12)


#define FILES (1000)
#define SIZE (100)

int
main(int argc, const char *argv[]) 
{
    Debug_OpenLog("vnode_create.log");
    auto os = initOSD(1 << 30, {500 * MEGABYTES, 500 * MEGABYTES});
    auto f = [](CVNode * n) {
        n->truncate(SIZE * BLOCK_SIZE);
        SGArray arr = SGArray();
        auto str = std::to_string(n->stats().inum);
        for (int i = 0; i < SIZE; i++) {
            char * buff = (char *) malloc(BLOCK_SIZE);
            memset(buff, 0, BLOCK_SIZE);
            strncpy(buff, str.c_str(), sizeof(char) * str.size());
            auto freefunc = [buff](){ free(buff); };
            arr.add(BLOCK_SIZE, i * BLOCK_SIZE, (void *)buff, freefunc, nullptr);
        }
        n->write(arr);
        arr.free();
    };

    auto t = [](CVNode *n) {
        auto str = std::to_string(n->stats().inum);
        for (int i = 0; i < SIZE; i++) {
            SGArray arr = SGArray();
            char * buff = (char *) malloc(BLOCK_SIZE);
            auto freefunc = [buff](){ free(buff); };
            arr.add(BLOCK_SIZE, i * BLOCK_SIZE, (void *)buff, freefunc, nullptr);
            n->read(arr);
            auto e = arr.get(0);
            std::string s((char *)e->buffer);
            TEST_ASSERT(s == str);
            arr.free();
        }
    };
    
    createFiles(os, FILES);
    actionFile(os, FILES, f);
    actionFile(os, FILES, t);
    os->sync();
    actionFile(os, FILES, t);
    PStats_Log();
    
    return 0;
}
