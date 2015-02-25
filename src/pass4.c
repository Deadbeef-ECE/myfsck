#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

#include "fsck_util.h"
#include "partition.h"
#include "readwrite.h"
#include "ext2_fs.h"
#include "pass4.h"

void pass4_fix_block_bitmap(fsck_info_t* fsck_info)
{
	int groups_num = get_groups_num(&fsck_info->sblock);
	int block_size = get_block_size(&fsck_info->sblock);
	int inode_size = get_inode_size(&fsck_info->sblock);
	int blks_per_group = get_blks_per_group(&fsck_info->sblock);
	int inds_per_group = get_inds_per_group(&fsck_info->sblock);
	int total = block_size * blks_per_group;
	int i = 0;
	fsck_info->block_map = (int *)malloc(sizeof(int) * total);
	if(fsck_info->block_map == NULL){
		fprintf(stderr, "ERROR: Allocate memory failed for block_map\n");
		return;
	}
	init_local_blkmap(fsck_info, total);
	int metadata_size = 0;

	// First two blocks are superblock and boot record block
	metadata_size += 2048;
	// Add the size of block group descriptor table
	metadata_size +=  sizeof(grp_desc_t) * groups_num;
	int metadata_num = (metadata_size - 1)/block_size + 1;

	for(i = 0; i < metadata_num; i++){
		fsck_info->block_map[i] = 1;
	}

	for(i = 0; i < groups_num; i++){
		/* Block bitmap occupy one block */
		int blk_bitmap_addr = fsck_info->blkgrp_desc_tb[i].bg_block_bitmap;
		fsck_info->block_map[blk_bitmap_addr] = 1;
		/* Inode bitmap occupy one block */
		int inode_bitmap_addr = fsck_info->blkgrp_desc_tb[i].bg_inode_bitmap;
		fsck_info->block_map[inode_bitmap_addr] = 1;

		/* Inode table occupy several blocks */
		int inode_tb_start = fsck_info->blkgrp_desc_tb[i].bg_inode_table;
		int inode_tb_sz = inode_size * inds_per_group;
		int num = (inode_tb_sz - 1)/block_size + 1;
		int inode_tb_end = inode_tb_start + num;
		for(i = inode_tb_start; i < inode_tb_end; i++){
			fsck_info->block_map[i] = 1;
		}
	}

	return;
}


void init_local_blkmap(fsck_info_t *fsck_info, int total)
{
	int i = 0;
	for(; i < total; i++){
		fsck_info->block_map[i] = 0;
	}
}