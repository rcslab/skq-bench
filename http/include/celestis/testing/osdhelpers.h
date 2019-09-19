/**
 * @author Kenneth R Hancock
 */ 
#ifndef __CELESTIS_TEST_OSDHELPERS_H__
#define __CELESTIS_TEST_OSDHELPERS_H__

#include <vector>

#include "../debug.h"
#include "../OSD/cvnode.h"
#include "../OSD/cosd.h"
#include "../OSD/disk.h"
#include "../OSD/memdisk.h"
#include "../OSD/diskosd.h"

DiskOSD * initOSD(size_t memory, const std::list<size_t> &sizes)
{
    std::vector<Disk *> disks;
    int i = 0;
    for (auto s : sizes) {
        disks.push_back(new MemDisk(s, i));
        i++;
    }
    DiskOSD * osd = new DiskOSD(memory);
    osd->initialize(disks);
    return osd;
}


void createFiles(CObjectStore *osd, size_t numFiles)
{
    for (size_t i = 0; i < numFiles; i++) {
        auto n = osd->create();
        n->close();
    }
}

void actionFile(CObjectStore *osd, size_t numFiles, std::function<void(CVNode *)> f)
{
    for (size_t i = 2;i < numFiles; i++) {
        auto n = osd->open(i);
        f(n); 
        n->close();
    }
}
#endif
// @}
