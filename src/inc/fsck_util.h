#ifndef _FSCK_UTIL_H_
#define _FSCK_UTIL_H_

#include "partition.h"
#include "ext2_fs.h"

#define SECT_SIZE	512

/** Macros for MBR */
#define PT_E_NUM	4
#define TYPE_OFFSET	0x04
#define SECT_OFFSET 0x08
#define LEN_OFFSET	0x0c

#define PARSE_SUCC	0
#define PARSE_FAIL	-1

typedef fsck_info{
	sblock_t sblock;
	partition_t partition;
}fsck_info_t;


int parse_pt_info(partition_t *pt, uint32_t pt_num);
void print_pt_info(partition_t *pt);

#endif /* !_FSCK_UTIL_H_ */