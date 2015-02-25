/* @file: pass1.c
 *
 * @breif: Functions to validate pass1
 *
 * @author: Yuhang Jiang (yuhangj@andrew.cmu.edu)
 * @bug: No known bugs
 */

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

void pass1_correct_dir(fsck_info_t *fsck_info, uint32_t inode_num, uint32_t parent)
{
	clear_local_inode_map(fsck_info);
	trav_dir(fsck_info, inode_num, parent);
	return;
}

void trav_dir(fsck_info_t *fsck_info, uint32_t inode_num, uint32_t parent)
{
	inode_t inode;
	int inode_addr = compute_inode_addr(fsck_info, inode_num);

	/* get inode_addr of certain inode_num */
	read_bytes(inode_addr, sizeof(inode_t), &inode);

	/* Check if this is a directory or not */
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
		
		/* Read one direct block of inode */
		read_bytes(disk_offset, block_size, buf);

		/* Traversal the direct block */
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
			if(dir_entry.inode != current_dir)
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
			if(dir_entry.inode != parent_dir)
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
// 