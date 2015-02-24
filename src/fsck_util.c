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
#include "pass2.h"

//#define DEBUG_DESC_TABLE

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
	// printf("i:%d; cur_ebr_sect:%d, first_ebr_sect:%d\n", 
	// 	i, (int)cur_ebr_sect, (int) first_ebr_sect);

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

	//printf("cur_ebr_sect:%d, first_ebr_sect:%d\n", 
	//	(int)cur_ebr_sect, (int) first_ebr_sect);

	read_sectors(cur_ebr_sect, 1 * SECT_SIZE, buf);
	//print_sector(buf);
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

void do_fix(int fix_pt_num)
{
	fsck_info_t *fsck_info = malloc(sizeof(fsck_info_t));
	if(fsck_info == NULL){
		fprintf(stderr, "ERORR: Allocate memory for fsck_info failed!\n");
		return;
	}
	memset(fsck_info,0, sizeof(fsck_info_t));
	if(fsck_info_init(fsck_info, fix_pt_num) == -1){
		free(fsck_info);
		return;
	}
	
	int inodes_num = get_inodes_num(&fsck_info->sblock);
	fsck_info->inode_map = (int*)malloc(sizeof(int) * (1+inodes_num));
	
	if(fsck_info->inode_map == NULL){
		fprintf(stderr, "ERORR: Allocate memory for fsck_info->inode_map failed!\n");
		return;
	}

	clear_local_inode_map(fsck_info);
	trav_dir(fsck_info, EXT2_ROOT_INO, EXT2_ROOT_INO);
	
	pass2_fix_unref_inode(fsck_info);

	//printf("why?\n");
	clear_local_inode_map(fsck_info);
	trav_dir(fsck_info, EXT2_ROOT_INO, EXT2_ROOT_INO);

	return;
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


int parse_sblock(fsck_info_t* fsck_info, uint32_t pt_num)
{
	read_sectors(fsck_info->pt.start_sec + 1024/SECT_SIZE, 
		sizeof(sblock_t), &fsck_info->sblock);
	return 1;
}

int parse_blkgrp_desc_tb(fsck_info_t* fsck_info, uint32_t pt_num){
	if(fsck_info->blkgrp_desc_tb != NULL){
		fprintf(stdout, "free previous blkgrp_desc_tb\n");
		free(fsck_info->blkgrp_desc_tb);
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
    
    // printf("start_sec: %d\n", fsck_info->pt.start_sec + 2048/512);
    // printf("size: %d\n", size);

	//dump_grp_desc(fsck_info->blkgrp_desc_tb, size/sizeof(grp_desc_t));

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

void trav_dir(fsck_info_t *fsck_info, uint32_t inode_num, uint32_t parent)
{
	inode_t inode;
	int inode_addr = compute_inode_addr(fsck_info, inode_num);

	//get inode_addr of certain inode_num
	read_bytes(inode_addr, sizeof(inode_t), &inode);

	// Check if this is a directory or not
	if(!EXT2_ISDIR(inode.i_mode))
		return;

	int block_size = get_block_size(&fsck_info->sblock);
	uint8_t buf[block_size];
	int i = 0;
	for(; i < EXT2_NDIR_BLOCKS; i++){
		if(inode.i_block[i] <= 0)
			continue;

		int disk_offset = fsck_info->pt.start_sec * SECT_SIZE +
							inode.i_block[i] * block_size;
		
		// Read one direct block of inode
		read_bytes(disk_offset, block_size, buf);

		trav_direct_blk(fsck_info, disk_offset, i, buf, inode_num, parent);
	}
	/* Traversal indirect block */
	read_bytes(fsck_info->pt.start_sec * SECT_SIZE + 
				inode.i_block[EXT2_IND_BLOCK] * block_size, 
			    block_size, buf);
	trav_indirect_blk(fsck_info, (uint32_t*)buf, inode_num, parent);
	
	/* Traversal doubly indirect block */
	read_bytes(fsck_info->pt.start_sec * SECT_SIZE + 
				inode.i_block[EXT2_DIND_BLOCK] * block_size, 
			    block_size, buf);
	trav_dindirect_blk(fsck_info, (uint32_t*)buf, inode_num, parent);
	
	/* Traversal triply indirect block */
	read_bytes(fsck_info->pt.start_sec * SECT_SIZE + 
				inode.i_block[EXT2_TIND_BLOCK] * block_size, 
			    block_size, buf);
	trav_tindirect_blk(fsck_info, (uint32_t*)buf, inode_num, parent);

	return;
}

void trav_direct_blk(fsck_info_t *fsck_info, 
					int block_offset, 
					int iblock_num, 
					uint8_t* buf, 
					uint32_t current_dir, 
					uint32_t parent_dir)
{
	dir_entry_t dir_entry;

	int dir_entry_offset = 0;
	int block_size = get_block_size(&fsck_info->sblock);
	int inodes_num = get_inodes_num(&fsck_info->sblock);
	//printf("inodes_num:%d\n", inodes_num);
	int cnt = 1;
	while(dir_entry_offset < block_size)
	{
		/* no more directory entries in this block */
		dir_entry.inode = *(__u32*)(buf + dir_entry_offset);
		dir_entry.rec_len = *(__u16*)(buf + dir_entry_offset + REC_LEN_OFFSET);
		dir_entry.name_len = *(__u8*)(buf + dir_entry_offset + NAME_LEN_OFFSET);
		dir_entry.file_type = *(__u8*)(buf + dir_entry_offset + FILE_TYPE_OFFSET);
		memcpy(dir_entry.name, buf + dir_entry_offset + NAME_OFFSET, dir_entry.name_len);
		dir_entry.name[dir_entry.name_len + 1] = '\0';
		
		/* check '.' entry */
		if (cnt == 1 && iblock_num == 0)
		{
			if(//strcmp(dir_entry.name, ".") != 0 || 
			          dir_entry.inode != current_dir)
			{
				printf("error in \".\" of dir %d should be %d\n", 
				        dir_entry.inode, current_dir);
				/* write back to disk */
				write_bytes(block_offset + dir_entry_offset,
						sizeof(uint32_t), &current_dir);

				/* update block buf */
				*(__u32*)(buf + dir_entry_offset) = current_dir;
			}
		}
		/* check '..' entry */
		if (cnt == 2 && iblock_num == 0)
		{
			if(//strcmp(dir_entry.name, "..") != 0 || 
			          dir_entry.inode != parent_dir)
			{
				printf("error \"..\" in dir %d, should be %d\n", 
				        current_dir, parent_dir);
				/* write back to disk */
				write_bytes(block_offset + dir_entry_offset,
						sizeof(uint32_t), &parent_dir);

				/* update block buf */
				*(__u32*)(buf + dir_entry_offset) = parent_dir;
			}
		}
		/* get inode again after possible fix */
		dir_entry.inode = *(__u32*)(buf + dir_entry_offset);
		
		
		/* update local inode map */
		if (dir_entry.inode <= inodes_num)
		{
			fsck_info->inode_map[dir_entry.inode] += 1;
		}
		if(dir_entry.inode > inodes_num){
			fprintf(stderr, "WTF????????????\n");
		}
		
		/* recursively traverse sub-directory in this folder */
		if (dir_entry.file_type == EXT2_FT_DIR
		  && fsck_info->inode_map[dir_entry.inode] <= 1
		  && (cnt>2 || iblock_num > 0) )
			trav_dir(fsck_info, dir_entry.inode, current_dir);
		
		dir_entry_offset += dir_entry.rec_len;
		cnt++;
	}
	return;
}

void trav_indirect_blk( fsck_info_t* fsck_info,
						uint32_t* singly_buf, 
                    	uint32_t current_dir, 
                    	uint32_t parent_dir)
{
	int block_size = get_block_size(&fsck_info->sblock);
	uint8_t direct_buf[block_size];
	int i = 0;
	for(; i < (block_size / 4); i++)
	{
		if (singly_buf[i] == 0)
			break;

		int disk_offset = fsck_info->pt.start_sec * SECT_SIZE 
						+ singly_buf[i] * block_size;
		read_bytes(disk_offset, block_size, direct_buf);

		trav_direct_blk(fsck_info, disk_offset, 1, direct_buf, 
						current_dir, parent_dir);
	}
	return;
}


void trav_dindirect_blk(fsck_info_t* fsck_info,
						uint32_t* doubly_buf, 
                    	uint32_t current_dir, 
                    	uint32_t parent_dir)
{
	int block_size = get_block_size(&fsck_info->sblock);
	uint32_t singly_buf[block_size];

	int i = 0;
	for(; i < (block_size / 4); i++)
	{
		if (doubly_buf[i] == 0)
			break;
		int disk_offset = fsck_info->pt.start_sec * SECT_SIZE 
						+ doubly_buf[i] * block_size;

		read_bytes(disk_offset, block_size, singly_buf);

		trav_indirect_blk(fsck_info, singly_buf, current_dir, parent_dir);
	}
	return;
}

void trav_tindirect_blk(fsck_info_t* fsck_info,
						uint32_t* triply_buf, 
                     	uint32_t current_dir, 
                     	uint32_t parent_dir)
{
	int block_size = get_block_size(&fsck_info->sblock);
	uint32_t doubly_buf[block_size];
	int i = 0;
	for(; i < (block_size / 4); i++)
	{
		if (triply_buf[i] == 0)
			break;
		int disk_offset = fsck_info->pt.start_sec * SECT_SIZE 
						+ triply_buf[i] * block_size;

		read_bytes(disk_offset, block_size, doubly_buf);
		trav_dindirect_blk(fsck_info, doubly_buf, current_dir, parent_dir);
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

