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

#define INIT_SUCC	0
#define INIT_FAIL	-1

typedef struct fsck_info{
	sblock_t sblock;
	partition_t pt;
	grp_desc_t *blkgrp_desc_tb;
	int* inode_map;
	int* block_map;
	uint8_t* bitmap;
}fsck_info_t;


int parse_pt_info(partition_t *pt, uint32_t pt_num);
void print_pt_info(partition_t *pt);
int parse_sblock(fsck_info_t* fsck_info, uint32_t pt_num);
int parse_blkgrp_desc_tb(fsck_info_t* fsck_info, uint32_t pt_num);
int fsck_info_init(fsck_info_t *fsck_info, uint32_t pt_num);
void do_fix(int fix_pt_num);

int get_block_size(sblock_t *sblock);
int get_inode_size(sblock_t *sblock);
int get_blocks_num(sblock_t *sblock);
int get_blks_per_group(sblock_t *sblock);
int get_inodes_num(sblock_t *sblock);
int get_inds_per_group(sblock_t *sblock);
int get_groups_num(sblock_t *sblock);
void debug_sblock(fsck_info_t *fsck_info);
void dump_grp_desc(grp_desc_t *entry);


#endif /* !_FSCK_UTIL_H_ */