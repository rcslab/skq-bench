/*
 * @ingroup ObjectStoreDevice
 *
 * @class Disk
 *
 * @brief Disk is the pure virtual class that allows us to abstract out types 
 * of disks to the ObjectStore device.
 *
 * @author Kenneth R Hancock
 */
#ifndef __CELESTIS_FS_DISK_H__
#define __CELESTIS_FS_DISK_H__

#include <stdint.h>
#include <unistd.h>
#include <sys/uio.h>
#include <string>

#include "sgarray.h"

class Disk 
{
    public:
        Disk(){};
        /*
         * Standard read function used for the disk object
         *
         * @param buffer Pointer to the memory buffer
         * @param len Length of the data at the memory buffer
         * @param offset Offset to write the data 
         */
        virtual ssize_t read(char *buffer, size_t len, off_t offset)=0;

        /*
         * Standard read function used for the disk object
         *
	 * @param arr scatter_gather array for IOS 
         */
        virtual ssize_t read(SGArray &arr, off_t offset)=0;

        /*
         * Standard read function used for the disk object
         *
	 * @param arr scatter_gather array for IOS
         */
        virtual ssize_t write(SGArray &arr,off_t offset)=0;

        /*
         * Standard read function used for the disk object
         *
         * @param buffer Pointer to the memory buffer
         * @param len Length of the data at the memory buffer
         * @param offset Offset to write the data 
         */
        virtual ssize_t write(char *buffer, size_t len, off_t offset)=0;

        /*
         * Standard read function used for the disk object
         *
         * @param buffer Pointer to the memory buffer
         * @param len Length of the data at the memory buffer
         */
        virtual ssize_t pwritev(iovec *io, size_t iovcnt, off_t offset)=0;
    
        /*
         * get_size retrieves the full size of the disk
         */
        virtual size_t get_size()=0;
        /*
         * get_id retrieves the identification number of the disk
         */
        virtual uint32_t get_id()=0;

        /*
         * get_asize retrieves the actual size of the disk, meaning the total 
         * USABLE space of the disk.  This is the data that can be broken up 
         * into blocks and used by the file system.
         */
        virtual size_t get_asize()=0;
};

#endif /* __CELESTIS_FS_DISK_H__ */


