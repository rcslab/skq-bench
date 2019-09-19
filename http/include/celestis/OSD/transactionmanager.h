#ifndef __CELESTIS_FS_TRANSACTIONMANAGER_H__
#define __CELESTIS_FS_TRANSACTIONMANAGER_H__

#include <unordered_set>
#include <queue>

#include "../pstats.h"
#include "transactiongroup.h"
#include "diskosd.h"

#define COUNTDOWN 2

/*
 * Transaction Manager acts as the manager of the transaction groups, when 
 * syncs are called it will stop writes and flush dirty nodes from the current
 * transaction group and then swaps it out when done with a new transaction 
 * groups
 */
class TransactionManager {
    private:
        queue<vector<blkptr>> expiring;
        DiskOSD *os; 

        void update_chain(VirtualBlkPtr &current, DiskMap &write_arr, bool forced = false);
        void create_writes(VNode *v, DiskMap &write_arr);
        void countdown();
        void create_and_modify(VirtualBlkPtr &p, DiskMap &write_arr, dva address);
        void update_meta(VNode *v);
        DiskMap write_inodes();
        DiskMap write_reserved();
    protected:
        void expire();
        friend class TransactionGroup;
    public:
        TransactionManager(DiskOSD *s);
        ~TransactionManager();
        void create_new_transaction();
        std::string to_string(int tab_spacing = 0);
};

#endif  /* __CELESTIS_FS_TRANSACTIONMANAGER_H__ */

