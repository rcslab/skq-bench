#ifndef __CELESTIS_FS_TRANSACTIONGROUP_H__
#define __CELESTIS_FS_TRANSACTIONGROUP_H__

#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "calloc.h"
#include "vblkptr.h"
#include "diskosd.h"

/*
 * Transaction groups are the fundemental component to our copy on write style 
 * system, it holds within its maps dirty inodes, and dirty blocks which need 
 * to be flushed to disk
 */
class TransactionGroup {
    private:
        DiskOSD * os;
    public:
        TransactionGroup(DiskOSD * os);
        uint64_t txg_num;
        unordered_set<VNode *> dirty;

        void add(VNode * node);
        vector<write_e> create_writes(VNode * v);
        std::string to_string(int tab_spacing = 0);

        typedef unordered_set<VNode *>::iterator iterator;
        iterator begin() {

            return dirty.begin();
        }
        iterator end() {

            return dirty.end();
        }
};

#endif



