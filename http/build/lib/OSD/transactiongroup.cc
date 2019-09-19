#include <string>
#include <sstream>

#include <celestis/debug.h>
#include <celestis/OSD/util.h>
#include <celestis/OSD/transactiongroup.h>
#include <celestis/OSD/transactionmanager.h>
#include <celestis/OSD/diskvnode.h>

TransactionGroup::TransactionGroup(DiskOSD * os) : os(os)
{
}

void
TransactionGroup::add(VNode * v)
{
    // this is super ugly. But only the sync should force a flush of this vnode, and it should already have 
    // the lock
    if (v->inode_num != 1) {
        std::unique_lock<std::mutex> lock(os->txg_lock); 
    }
    DLOG("TXG %lu: Write of %s", txg_num, v->to_string().c_str());
    if (dirty.find(v) == dirty.end()) {
        dirty.insert(v);
        v->retain();
    }
}

std::string
TransactionGroup::to_string(int tab_spacing)
{
    std::stringstream ss;
    ss << "TXG " << this->txg_num << std::endl;
    for (auto &it : this->dirty) {
        ss << "\tInode Number:" << it->inode_number() << std::endl;
        ss << std::endl;
    }

    return tab_prepend(ss, tab_spacing);
}
