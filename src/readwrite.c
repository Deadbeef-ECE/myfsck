/* @file: readwrite.c
 *
 * @breif: Code to read and write sectors to a "disk" file.
 *         This is a support file for the "fsck" storage systems laboratory.
 *
 * @author: Yuhang Jiang (yuhangj@andrew.cmu.edu)
 * @bug: No known bugs
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>     /* for memcpy() */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

#if defined(__FreeBSD__)
#define lseek64 lseek
#endif

/* linux: lseek64 declaration needed here to eliminate compiler warning. */
extern int64_t lseek64(int, int64_t, int);
extern int device;
const unsigned int sector_size_bytes = 512;

/* print_sector: print the contents of a buffer containing one sector.
 *
 * inputs:
 *   char *buf: buffer must be >= 512 bytes.
 *
 * outputs:
 *   the first 512 bytes of char *buf are printed to stdout.
 *
 * modifies:
 *   (none)
 */
void print_sector (unsigned char *buf)
{
    int i;
    for (i = 0; i < sector_size_bytes; i++) {
        printf("%02x", buf[i]);
        if (!((i+1) % 32))
            printf("\n");      /* line break after 32 bytes */
        else if (!((i+1) % 4))
            printf(" ");   /* space after 4 bytes */
    }
}


/* read_sectors: read a specified number of sectors into a buffer.
 *
 * inputs:
 *   int64 start_sector: the starting sector number to read.
 *                       sector numbering starts with 0.
 *   int numsectors: the number of sectors to read.  must be >= 1.
 *   int device [GLOBAL]: the disk from which to read.
 *
 * outputs:
 *   void *into: the requested number of sectors are copied into here.
 *
 * modifies:
 *   void *into
 */
void read_sectors (int64_t start_sector, ssize_t len, void *into)
{
    ssize_t ret;
    int64_t lret;
    int64_t sector_offset;

    sector_offset = start_sector * sector_size_bytes;

    if ((lret = lseek64(device, sector_offset, SEEK_SET)) != sector_offset) {
        fprintf(stderr, "Seek to position %"PRId64" failed: "
                "returned %"PRId64"\n", sector_offset, lret);
        exit(-1);
    }

    if ((ret = read(device, into, len)) != len) {
        fprintf(stderr, "Read sector %"PRId64" length %d failed: "
                "returned %"PRId64"\n", start_sector, (int)len, ret);
        exit(-1);
    }
}


/* write_sectors: write a buffer into a specified number of sectors.
 *
 * inputs:
 *   int64 start_sector: the starting sector number to write.
 *                	sector numbering starts with 0.
 *   int numsectors: the number of sectors to write.  must be >= 1.
 *   void *from: the requested number of sectors are copied from here.
 *
 * outputs:
 *   int device [GLOBAL]: the disk into which to write.
 *
 * modifies:
 *   int device [GLOBAL]
 */
void write_sectors (int64_t start_sector, unsigned int num_sectors, void *from)
{
    ssize_t ret;
    int64_t lret;
    int64_t sector_offset;
    ssize_t bytes_to_write;

    sector_offset = start_sector * sector_size_bytes;

    if ((lret = lseek64(device, sector_offset, SEEK_SET)) != sector_offset) {
        fprintf(stderr, "Seek to position %"PRId64" failed: "
                "returned %"PRId64"\n", sector_offset, lret);
        exit(-1);
    }

    bytes_to_write = sector_size_bytes * num_sectors;

    if ((ret = write(device, from, bytes_to_write)) != bytes_to_write) {
        fprintf(stderr, "Write sector %"PRId64" length %d failed: "
                "returned %"PRId64"\n", start_sector, num_sectors, ret);
        exit(-1);
    }
}

/** @brief read bytes from device
 *  
 *  @param base: base address 
 *  @param buf_len: length of bytes to read
 *  @param into: buffer to store read bytes
 */
void read_bytes(int64_t base, ssize_t buf_len, void* into)
{
    ssize_t ret;
    int64_t lret;
    
    if((lret = lseek64(device, base, SEEK_SET)) != base)
    {
        printf("Seek to position %ld failed:\n", base);
        exit(-1);
    }
  
    if((ret = read(device, into, buf_len)) != buf_len)
    {
        printf("Read device failed in read_bytes\n");
        exit(-1);
    }
}

/** @brief write bytes to device
 *  
 *  @param base: base address 
 *  @param buf_len: length of bytes to write
 *  @param from: source of bytes
 */
void write_bytes(int64_t base, ssize_t buf_len, void* from)
{
    ssize_t ret;
    int64_t lret;

    if((lret = lseek(device, base, SEEK_SET)) != base)
    {
        printf("Seek to position %ld failed:\n", base);
        exit(-1);
    }
    if((ret = write(device, from, buf_len)) != buf_len)
    {
        printf("Write device failed in write_bytes\n");
        exit(-1);
    }
}

/* EOF */
