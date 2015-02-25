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
#include "pass1.h"
#include "pass2.h"
#include "pass3.h"
#include "pass4.h"

// #define DEBUG_DESC_TABLE
// #define DEBUG_PARTITION

extern int device;

int parse_pt_info(partition_t *pt, uint32_t pt_num)
{
	if(pt_num > 0){
		pt->pt_num = pt_num;
	}else{
		return PARSE_FAIL;
	}	
	
	int i;
	int ebr_flag = 0;
	uint8_t buf[SECT_SIZE];
	uint32_t pt_entry_offset = 0;

	/** Read MBR */
	read_sectors(0, 1*SECT_SIZE, buf);
	
	if(pt_num <= PT_E_NUM){
		pt_entry_offset = PT_OFFSET + (pt_num - 1) * PT_ENTRY_SZ;
		pt->type = buf[pt_entry_offset + TYPE_OFFSET];
		pt->start_sec = *(uint32_t*)(buf+ pt_entry_offset+ SECT_OFFSET);
		pt->length =  *(uint32_t*)(buf+ pt_entry_offset+ LEN_OFFSET);
		return PARSE_SUCC;
	}

	for(i = 1; i <= PT_E_NUM; i++){
		if(EXTEND == buf[PT_OFFSET + PT_ENTRY_SZ *(i-1) + TYPE_OFFSET]){
			ebr_flag = 1;
			break;
		}
	}

	if(ebr_flag == 0){
		fprintf(stderr, "ERORR: Cannot find ebr!\n");
		return PARSE_FAIL;
	}

	uint32_t first_ebr_sect = *(uint32_t*)(buf + PT_OFFSET + 
		PT_ENTRY_SZ * (i-1) + SECT_OFFSET);
	uint32_t cur_ebr_sect = first_ebr_sect;

#ifdef DEBUG_PARTITION
	printf("i:%d; cur_ebr_sect:%d, first_ebr_sect:%d\n", 
		i, (int)cur_ebr_sect, (int) first_ebr_sect);
#endif

	for(i = 5; i < pt_num; i++){
		//Read the first ebr sector
		read_sectors(cur_ebr_sect, 1 * SECT_SIZE, buf);

		//entry 2 will be 0s means we reach the end of the ext blocks
		if((*(int64_t*)(buf + PT_OFFSET + PT_ENTRY_SZ)) == 0 &&
		   (*(int64_t*)(buf + PT_OFFSET + PT_ENTRY_SZ + 8)) == 0){
			if(i < pt_num)
				return PARSE_FAIL;
			break;
		}

		cur_ebr_sect = first_ebr_sect +
		 *(int*)(buf + PT_OFFSET + PT_ENTRY_SZ + SECT_OFFSET);
	}

#ifdef DEBUG_PARTITION
	printf("cur_ebr_sect:%d, first_ebr_sect:%d\n", 
		(int)cur_ebr_sect, (int) first_ebr_sect);
#endif

	read_sectors(cur_ebr_sect, 1 * SECT_SIZE, buf);

#ifdef DEBUG_PARTITION
	print_sector(buf);
#endif

	pt->type = buf[PT_OFFSET + TYPE_OFFSET];
	pt->start_sec = cur_ebr_sect + *(uint32_t*)(buf+ PT_OFFSET+ SECT_OFFSET);
	pt->length =  *(uint32_t*)(buf+ PT_OFFSET+ LEN_OFFSET);
	
	return PARSE_SUCC;
}

void print_pt_info(partition_t *pt)
{
	printf("0x%02X %d %d\n", 
		pt->type, (int)pt->start_sec, (int)pt->length);
}

void clear_local_inode_map(fsck_info_t *fsck_info){
	int i = 0;
	int inode_num = get_inodes_num(&fsck_info->sblock);
	for(; i <= inode_num; i++){
		fsck_info->inode_map[i] = 0;
	} 
	return;
}

int do_fix(int fix_pt_num)
{
	fsck_info_t *fsck_info = malloc(sizeof(fsck_info_t));
	if(fsck_info == NULL){
		fprintf(stderr, "ERORR: Allocate memory for fsck_info failed!\n");
		return FIX_FAIL;
	}
	memset(fsck_info,0, sizeof(fsck_info_t));
	if(fsck_info_init(fsck_info, fix_pt_num) == -1){
		destroy_fsck_info(fsck_info);
		return FIX_FAIL;
	}
	
	int inodes_num = get_inodes_num(&fsck_info->sblock);
	fsck_info->inode_map = (int*)malloc(sizeof(int) * (1+inodes_num));
	
	if(fsck_info->inode_map == NULL){
		fprintf(stderr, "ERORR: Allocate memory for fsck_info->inode_map failed!\n");
		destroy_fsck_info(fsck_info);
		return FIX_FAIL;
	}
	
	pass1_correct_dir(fsck_info, EXT2_ROOT_INO, EXT2_ROOT_INO);
	
	pass2_fix_unref_inode(fsck_info);

	pass3_fix_link_count(fsck_info);

	pass4_fix_block_bitmap(fsck_info);

	destroy_fsck_info(fsck_info);

	return FIX_SUCC;
}

int fsck_info_init(fsck_info_t *fsck_info, uint32_t pt_num)
{
	if (parse_pt_info(&fsck_info->pt, pt_num) == -1)
	{	
		fprintf(stderr, "ERROR: Parse partition[%d] info failed!\n", pt_num);
		return INIT_FAIL;
	}
	if (parse_sblock(fsck_info, pt_num) == -1)
	{
		fprintf(stderr, "ERROR: Parse sblock of pt[%d] failed!\n", pt_num);
		return INIT_FAIL;
	}
	debug_sblock(fsck_info);
	if ((parse_blkgrp_desc_tb(fsck_info, pt_num)) == -1)
	{
		fprintf(stderr, "ERROR: Read blkgrp descriptor table of pat[%d] failed!\n",pt_num);
		return INIT_FAIL;
	}
	return INIT_SUCC;
}

void destroy_fsck_info(fsck_info_t * fsck_info)
{
	if(fsck_info == NULL)
		return;
	if(fsck_info->bitmap != NULL)
		free(fsck_info->bitmap);
	if(fsck_info->block_map != NULL)
		free(fsck_info->block_map);
	if(fsck_info->inode_map != NULL)
		free(fsck_info->inode_map);
	if(fsck_info->blkgrp_desc_tb != NULL)
		free(fsck_info->blkgrp_desc_tb);
	free(fsck_info);
	return;
}

int parse_sblock(fsck_info_t* fsck_info, uint32_t pt_num)
{
	read_sectors(fsck_info->pt.start_sec + 1024/SECT_SIZE, 
		sizeof(sblock_t), &fsck_info->sblock);
	return 1;
}

int parse_blkgrp_desc_tb(fsck_info_t* fsck_info, uint32_t pt_num){
	if(fsck_info->blkgrp_desc_tb != NULL){
		fprintf(stdout, "free previous blkgrp_desc_tb\n");
		//free(fsck_info->blkgrp_desc_tb);
		return PARSE_FAIL;
	}

	int size = get_groups_num(&fsck_info->sblock) * sizeof(grp_desc_t);

	/* allocate block group descriptor */
	fsck_info->blkgrp_desc_tb = (grp_desc_t *)malloc(size);
	if (fsck_info->blkgrp_desc_tb == NULL){
		fprintf(stderr, "ERROR: Cannot allocate memory for blkgrp_desc_tb\n");
		return PARSE_FAIL;
	}

	/* read block group table from sector 4 of the partition */
	read_sectors(fsck_info->pt.start_sec + 2048/SECT_SIZE, 
	            size, fsck_info->blkgrp_desc_tb);

#ifdef DEBUG_DESC_TABLE
    printf("start_sec: %d\n", fsck_info->pt.start_sec + 2048/512);
    printf("size: %d\n", size);
	dump_grp_desc(fsck_info->blkgrp_desc_tb, size/sizeof(grp_desc_t));
#endif

	return PARSE_SUCC;
}

void dump_grp_desc(grp_desc_t *entry, int num)
{	
	int i = 0;
	for(; i < num; i++){
		printf("\n********** group descriptor[%d] **************\n", i);
		printf("** bg_block_bitmap = %d\n",(int) entry[i].bg_block_bitmap);
		printf("** bg_inode_bitmap = %d\n", (int)entry[i].bg_inode_bitmap);
		printf("** bg_inode_table = %d\n", (int)entry[i].bg_inode_table);
		printf("** bg_free_blocks_count = %d\n", (int)entry[i].bg_free_blocks_count);
		printf("** bg_free_inodes_count = %d\n", (int)entry[i].bg_free_inodes_count);
		printf("** bg_used_dirs_count = %d\n", (int)entry[i].bg_used_dirs_count);
		printf("********************************************\n\n");
	}
	
	return;
}

int compute_inode_addr(fsck_info_t *fsck_info, uint32_t inode_num)
{
	//start from 1
	int inodes_per_group = get_inds_per_group(&fsck_info->sblock);
	int group_idx = (inode_num - 1) / inodes_per_group;
	int inode_idx = (inode_num - 1) % inodes_per_group;

	int pt_base = fsck_info->pt.start_sec * SECT_SIZE;
	int table_offset = get_block_size(&fsck_info->sblock)
				*fsck_info->blkgrp_desc_tb[group_idx].bg_inode_table;
	int inode_offset = inode_idx * get_inode_size(&fsck_info->sblock);

	int ret = pt_base + table_offset + inode_offset;
	
	return ret;
}

int get_block_size(sblock_t *sblock)
{
	return EXT2_BLOCK_SIZE(sblock);
}

int get_inode_size(sblock_t *sblock)
{
	return EXT2_INODE_SIZE(sblock);
}

int get_blocks_num(sblock_t *sblock)
{
	return sblock->s_blocks_count;
}

int get_blks_per_group(sblock_t *sblock)
{
	return EXT2_BLOCKS_PER_GROUP(sblock);
}

int get_inodes_num(sblock_t *sblock)
{
	return sblock->s_inodes_count;
}

int get_inds_per_group(sblock_t *sblock)
{
	return EXT2_INODES_PER_GROUP(sblock);
}

int get_groups_num(sblock_t *sblock)
{
	return (get_blocks_num(sblock)-1)/get_blks_per_group(sblock) + 1;
}

void debug_sblock(fsck_info_t *fsck_info)
{
	printf("\n************ partition[%d] ****************\n", 
		fsck_info->pt.pt_num);
	printf("** start sector = %d  base = %d\n", fsck_info->pt.start_sec, 
		fsck_info->pt.start_sec * 512);
	printf("** block size = %d\n", get_block_size(&fsck_info->sblock));
	printf("** inode size = %d\n**\n", get_inode_size(&fsck_info->sblock));
	printf("** number of blocks = %d\n", get_blocks_num(&fsck_info->sblock));
	printf("** blocks per group = %d\n**\n", get_blks_per_group(&fsck_info->sblock));
	printf("** number of inodes = %d\n", get_inodes_num(&fsck_info->sblock));
	printf("** inodes per group = %d\n**\n", get_inds_per_group(&fsck_info->sblock));
	printf("** number of groups = %d\n", get_groups_num(&fsck_info->sblock));
	printf("********************************************\n\n");
	return;
}