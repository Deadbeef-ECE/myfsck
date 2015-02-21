#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

#include "partition.h"
#include "fsck_util.h"

extern const unsigned int sector_size_bytes;

int device;

void print_usage(){
    fprintf(stdout, "Usage: ./myfsck -p <partition number> -i /path/to/disk/image\n");
    return;
}

int main (int argc, char **argv)
{
    printf("Hello from myfsck!\n");
    
    char* disk_name = NULL;
    uint32_t pt_num = 0;
    int c;

    while((c = getopt(argc, argv, ":p:i")) != -1){
        switch(c){
            case 'p':
                pt_num = atoi(optarg);
                break;
            case 'i':
                disk_name = argv[optind];
                break;
            default:
                fprintf(stderr, "ERROR: Invalid input!\n");
                exit(-1);
        }
    }
    if(disk_name == NULL){
        print_usage();
        exit(-1);
    }

    fprintf(stdout, "get pt_num[%d] and disk_name[%s]\n", pt_num, disk_name);

    if((device = open(disk_name, O_RDWR)) == -1){
        fprintf(stderr, "ERROR: Cannot open the file!\n");
        exit(-1);
    }

    if(pt_num > 0){
        partition_t pt;
        if(parse_pt_info(&pt, pt_num) == -1){
            close(device);
            fprintf(stdout, "-1\n");
            exit(-1);
        }
        print_pt_info(&pt);
    }
    
    return 0;
}

