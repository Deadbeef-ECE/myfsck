#include <stdio.h>
#include <stdlib.h>
#include <string.h>     /* for memcpy() */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

extern const unsigned int sector_size_bytes;

int main (int argc, char **argv)
{
    /* This is a sample program.  If you want to print out sector 57 of
     * the disk, then run the program as:
     *
     *    ./readwrite disk 57
     *
     * You'll of course want to replace this with your own functions.
     */

    // unsigned char buf[sector_size_bytes];        /* temporary buffer */
    // int           the_sector;                     /* IN: sector to read */

    // if ((device = open(argv[1], O_RDWR)) == -1) {
    //     perror("Could not open device file");
    //     exit(-1);
    // }

    // the_sector = atoi(argv[2]);
    // printf("Dumping sector %d:\n", the_sector);
    // read_sectors(the_sector, 1, buf);
    // print_sector(buf);

    // close(device);
    printf("Hello from myfsck\n");
    return 0;
}