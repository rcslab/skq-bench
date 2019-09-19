/**
 * @file cosd.h
 * @defgroup ObjectStoreDevice
 *
 * @{
 *
 * @class COSD
 *
 * @brief A pure virtual class of Object storage devices
 *
 * @author Kenneth R Hancock
 * 
 */
#ifndef __CELESTIS_FS_COSD_H__
#define __CELESTIS_FS_COSD_H__

#include <stdint.h>

#include "cvnode.h"


struct OSDStat {
    uint64_t fspace; 
    uint64_t space;
    uint64_t bc_size;
};

class CObjectStore{
    public:
        /**
         * Base constructor for allocator class
         */
        CObjectStore() {};


        virtual ~CObjectStore() {};


        /**
         * open will open a specific VNode i.
         *
         * @note This will increment the reference count of the VNode.  Its 
         * important that the user calls close() on the object for proper
         * memory recollection.
         *
         * @param i The VNode to open.
         */
        [[nodiscard]]
        virtual CVNode * open(CNode i)=0;


        /**
         * create will create a new VNode..
         *
         * @note This will increment the reference count of the VNode.  So Its 
         * important that the user calls close() on the object for proper
         * memory recollection.
         *
         * @param i The VNode to open.
         */
        [[nodiscard]]
        virtual CVNode * create()=0;

        /**
         * Informs the TransactionManager to lock resources and commit the 
         * current transaction group to disk.
         */
        virtual void sync()=0;
               
        /**
         * Will remove the VNode i and begin the process of reclaiming on disk 
         * memory (If VNode has been committed to disk through sync).
         *
         * @note This will also reclaim all BlockEntry attached to this VNode.
         *
         * @param i The VNode to remove
         */

        virtual void remove(uint64_t i)=0;

        /**
         * Returns current stats of objectstore.
         */
        virtual OSDStat stats()=0;

        /**
         * The to_string function for the OSD
         * 
         * @return string representation of the state of the OSD
         */
        virtual std::string to_string(int tab_spacing = 0)=0;
};
#endif
// @}
