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
	printf("pass4\n");
	int groups_num = get_groups_num(&fsck_info->sblock);
	int block_size = get_block_size(&fsck_info->sblock);
	int inode_size = get_inode_size(&fsck_info->sblock);
	int blks_per_group = get_blks_per_group(&fsck_info->sblock);
	int inds_per_group = get_inds_per_group(&fsck_info->sblock);
	int inodes_num = get_inodes_num(&fsck_info->sblock);
	int blocks_num = get_blocks_num(&fsck_info->sblock);

	int total = groups_num * blks_per_group;

	int i = 0;
	int j = 0;
	fsck_info->block_map = (int *)malloc(sizeof(int) * total);
	if(fsck_info->block_map == NULL){
		fprintf(stderr, "ERROR: Allocate memory failed for block_map\n");
		//destroy_fsck_info(fsck_info);
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
		printf("i:%d, groups_num:%d\n", i, groups_num);
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
		for(j = inode_tb_start; j < inode_tb_end; j++){
			fsck_info->block_map[j] = 1;
		}
		/* mark superblock and bg descriptor table backup blocks */
		if (i==1)
		{
			fsck_info->block_map[1 + i * blks_per_group] = 1;
			fsck_info->block_map[1 + i * blks_per_group + 1] = 1;
		}
	}

	for(i = 1; i <= inodes_num; i++){
		if(fsck_info->inode_map[i] <= 0)
			continue;
		mark_block(fsck_info, i);
	}

	fsck_info->bitmap = (uint8_t *)malloc(block_size);
	if(fsck_info->bitmap == NULL){
		fprintf(stderr, "ERROR: Allocate memory for fsck_info->bitmap failed!\n");
		//destroy_fsck_info(fsck_info);
		return;
	}
	int num = 0;
	int cur_group_num = 0;

	for(num = blocks_num; num >0;)
	{
		int last = get_last_blk_bum(num, blks_per_group);

		int disk_offset = fsck_info->pt.start_sec * SECT_SIZE + 
		fsck_info->blkgrp_desc_tb[cur_group_num].bg_block_bitmap * block_size;

		read_bytes(disk_offset, block_size, fsck_info->bitmap);
		for(i = 0; i < last; i++){
			int block_index = cur_group_num * blks_per_group + i + FIRST_BLK_OFFSET;

			if(check_bit_map(fsck_info, i) 
				!= check_local_blk_map(fsck_info, block_index))
			{
				printf("ERROR: Block bitmap %d in group %d wrong, got %d\n",
					    i, cur_group_num, fsck_info->block_map[block_index - 1]);
				fsck_info->bitmap[i/8] = correct_bit_map(fsck_info, i) | correct_block_map(fsck_info, block_index, i);

			}
		}
		write_bytes((int64_t)disk_offset, block_size, fsck_info->bitmap);
		cur_group_num++;
		num -= blks_per_group;
	}

	return;
}

uint8_t check_bit_map(fsck_info_t *fsck_info, int block_index)
{
	if((fsck_info->bitmap[block_index/8] &(1<<(block_index%8))) == 0)
		return 1;
	return 0;
}

uint8_t correct_bit_map(fsck_info_t *fsck_info, int block_index){
	return fsck_info->bitmap[block_index/8] & (~(1<<(block_index%8)));
}

uint8_t correct_block_map(fsck_info_t *fsck_info, int block_index, int i){
	return fsck_info->block_map[block_index]<<(i%8);
}

uint8_t check_local_blk_map(fsck_info_t *fsck_info, int block_index)
{
	if(fsck_info->block_map[block_index] == 0)
		return 1;
	
	return 0;
}

int get_last_blk_bum(int num, int blks_per_group){
	if(num < blks_per_group)
		return num - FIRST_BLK_OFFSET;
	return blks_per_group;
}

void init_local_blkmap(fsck_info_t *fsck_info, int total)
{
	int i = 0;
	for(; i < total; i++){
		fsck_info->block_map[i] = 0;
	}
}

void mark_block(fsck_info_t *fsck_info, int inode_num)
{
	inode_t inode;
	int i = 0;
	int inode_addr = 0;
	int block_size = get_block_size(&fsck_info->sblock);
	
	/* Compute the inode address (in byte) from inode number */
	inode_addr = compute_inode_addr(fsck_info, inode_num);
	
	/* Read inode information from inode table entry */
	read_bytes(inode_addr, sizeof(inode_t), &inode);

	/* Don't count symbolic link file shorter than 60 bytes */
	if (inode.i_size < SYMLNK_SIZE && EXT2_ISLNK(inode.i_mode) )
		return;

	uint8_t buf[block_size]; 
	
	/* Search in direct blocks */
	for(; i < EXT2_NDIR_BLOCKS; i++)
	{
		if (inode.i_block[i] <= 0)
			continue;
		
		fsck_info->block_map[inode.i_block[i]] = 1;
	}
	/* Traversal indirect block */
	if (inode.i_block[EXT2_IND_BLOCK] > 0)
	{
		
		fsck_info->block_map[inode.i_block[EXT2_IND_BLOCK]] = 1;
		int disk_offset = fsck_info->pt.start_sec * SECT_SIZE + 
						inode.i_block[EXT2_IND_BLOCK] * block_size;
		read_bytes(disk_offset, block_size, buf);
		mark_indirect_block(fsck_info, (uint32_t*)buf);
	}
	/* Traversal dindirect block */

	if (inode.i_block[EXT2_DIND_BLOCK] > 0)
	{
		fsck_info->block_map[inode.i_block[EXT2_DIND_BLOCK]] = 1;
		int disk_offset = fsck_info->pt.start_sec * SECT_SIZE + 
						inode.i_block[EXT2_DIND_BLOCK] * block_size;
		read_bytes(disk_offset, block_size, buf);
		mark_dindirect_block(fsck_info, (uint32_t*)buf);
	}

	/* Traversal tindirect block */
	if (inode.i_block[EXT2_TIND_BLOCK] > 0)
	{
		fsck_info->block_map[inode.i_block[EXT2_TIND_BLOCK]] = 1;
		int disk_offset = fsck_info->pt.start_sec * SECT_SIZE + 
						inode.i_block[EXT2_TIND_BLOCK] * block_size;
		read_bytes(disk_offset, block_size, buf);
		mark_tindirect_block(fsck_info, (uint32_t*)buf);
	}
	return;
}

void mark_indirect_block(fsck_info_t *fsck_info, uint32_t* indirect_buf)
{
	int i = 0;
	int block_size = get_block_size(&fsck_info->sblock);
	
	for(; i < block_size / 4; i++)
	{
		if (indirect_buf[i] == 0)
			break;
		
		fsck_info->block_map[indirect_buf[i]] = 1;
	}

	return;
}

void mark_dindirect_block(fsck_info_t *fsck_info, uint32_t* dindirect_buf)
{
	int i = 0;
	int block_size = get_block_size(&fsck_info->sblock);
	uint32_t indirect_buf[block_size];
	
	for(; i < block_size / 4; i++)
	{
		if (dindirect_buf[i] == 0)
			break;

		fsck_info->block_map[dindirect_buf[i]] = 1;
		int disk_offset = fsck_info->pt.start_sec * SECT_SIZE + 
							block_size * dindirect_buf[i];
		read_bytes(disk_offset, block_size ,indirect_buf);

		mark_indirect_block(fsck_info, indirect_buf);
	}
	return;
}

void mark_tindirect_block(fsck_info_t *fsck_info, uint32_t* tindirect_buf)
{
	int i = 0;
	int block_size = get_block_size(&fsck_info->sblock);
	uint32_t dindirect_buf[block_size];
	for(; i < block_size / 4; i++)
	{
		if (tindirect_buf[i] == 0)
			break;

		fsck_info->block_map[tindirect_buf[i]] = 1;
		int disk_offset = fsck_info->pt.start_sec * SECT_SIZE + 
						block_size * tindirect_buf[i];
		read_bytes(disk_offset, block_size, dindirect_buf);

		mark_dindirect_block(fsck_info, dindirect_buf);
	}
	return;
}
