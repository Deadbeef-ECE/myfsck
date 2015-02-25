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
#include "pass1.h"

// #define DEBUG_PASS2

void pass2_fix_unref_inode(fsck_info_t *fsck_info)
{
	int inode_addr;
	inode_t inode;
	int i;
	int parent_inode = 0;
	int inodes_num = get_inodes_num(&fsck_info->sblock);
	/* get number of referenced inodes */
	for (i = 1; i <= inodes_num; i++)
	{
		/* get inode addr (in byte) from inode number */
		inode_addr = compute_inode_addr(fsck_info, i);
		
		/* read inode information from inode table entry */
		read_bytes(inode_addr, sizeof(inode_t), &inode);
		
		if (fsck_info->inode_map[i] == 0 && inode.i_links_count > 0)
		{
			// If not a directory, put it into lost+found
			if (!EXT2_ISDIR(inode.i_mode)){
				printf("Find ERROR: putting %d into /lost+found\n", i);
				add_lostfound(fsck_info, i);
			}else{
				parent_inode = get_parent_inode(fsck_info, &inode);
				/* get inode addr (in byte) from inode number */
				int p_inode_addr = compute_inode_addr(fsck_info, parent_inode);
				inode_t p_inode;
				/* read inode information from inode table entry */
				read_bytes(p_inode_addr, sizeof(inode_t), &p_inode);
				if(!(fsck_info->inode_map[parent_inode] == 0 
					&& p_inode.i_links_count > 0)){
					printf("Find ERROR: putting[%d] into /lost+found\n", i);
					add_lostfound(fsck_info, i);
				}
			}
		}
	}
	return;
}

int get_parent_inode(fsck_info_t *fsck_info, inode_t* inode)
{
	/* Block_size is 1024 bytes */
	int block_size = get_block_size(&fsck_info->sblock);
	uint8_t buf[block_size]; 
	int disk_offset = fsck_info->pt.start_sec * SECT_SIZE 
					+ inode->i_block[0] * block_size;
	/* Read first block of inode */
	read_bytes(disk_offset, block_size, buf);

	dir_entry_t dir_entry;
	int dir_entry_offset = 0;

	/* Skip directory entry . */
	dir_entry.rec_len = *(__u16*)(buf + dir_entry_offset + REC_LEN_OFFSET);
	dir_entry_offset += dir_entry.rec_len;

	/* Get directory entry .. */
	dir_entry.inode = *(__u32*)(buf + dir_entry_offset);

	return dir_entry.inode;
}

void dump_dir_entry(dir_entry_t dir_entry)
{	
	printf("\n********** dir_entry **************\n");
	printf("** dir_entry.inode = %d\n",(int) dir_entry.inode);
	printf("** dir_entry.name_len = %d\n", (int) dir_entry.name_len);
	printf("** dir_entry.file_type = %d\n", (int) dir_entry.file_type);
	printf("** dir_entry.rec_len = %d\n", (int) dir_entry.rec_len);
	//printf("** dir_entry.name = %s\n", (int)entry[i].bg_free_inodes_count);
	printf("********************************************\n");
	
	return;
}

int add_lostfound(fsck_info_t *fsck_info, int inode_num)
{
	inode_t inode;
	int inode_addr = 0;
	int block_size = get_block_size(&fsck_info->sblock);
	/* get inode addr (in byte) from inode number */
	inode_addr = compute_inode_addr(fsck_info, inode_num);
	
	/* read inode information from inode table entry */
	read_bytes(inode_addr, sizeof(inode_t), &inode);

	dir_entry_t dir_entry;
	/* inode number */
	dir_entry.inode = inode_num;
	/* name is inode_number */
	sprintf(dir_entry.name, "%d%c", inode_num, '\0');
	dir_entry.name_len = strlen(dir_entry.name) - 1;
	
	/* type */
	dir_entry.file_type = gen_filetype(inode.i_mode);

	int lostfoud_inode = get_inode(fsck_info, "/lost+found");
	uint32_t base = get_dir_entry_addr(fsck_info, lostfoud_inode, NAME_OFFSET + dir_entry.name_len);
	//printf("lostfoud_inode:%d base:%d\n", lostfoud_inode, (int)base);
	int pt_base = fsck_info->pt.start_sec * SECT_SIZE;
	dir_entry.rec_len = (base - pt_base - 1)/ block_size * block_size 
						+ block_size - base + pt_base;
	
	if (base < 0)
		return -1;

	int entry_size = NAME_OFFSET + dir_entry.name_len;

#ifdef DEBUG_PASS2
	dump_dir_entry(dir_entry);
	fprintf(stdout, "entrysize:%d\n\n\n", entry_size);
#endif

	write_bytes(base, entry_size, &dir_entry);

	return 0;
}

int gen_filetype(__u16 i_mode)
{	
	switch(i_mode&0xf000){
		case EXT2_S_IFSOCK:
			return EXT2_FT_SOCK;
		case EXT2_S_IFLNK:
			return EXT2_FT_SYMLINK;
		case EXT2_S_IFREG:
			return EXT2_FT_REG_FILE;
		case EXT2_S_IFBLK:
			return EXT2_FT_BLKDEV;
		case EXT2_S_IFDIR:
			return EXT2_FT_DIR;
		case EXT2_S_IFCHR:
			return EXT2_FT_CHRDEV;
		case EXT2_S_IFIFO:
			return EXT2_FT_FIFO;
		default:
			fprintf(stderr, "WTF?????\n");
			return EXT2_FT_UNKNOWN;
	}
}

int get_inode(fsck_info_t *fsck_info, const char* filepath)
{
	int len = strlen(filepath);
	char path[len+1];
	strcpy(path, filepath);
	path[len] = '\0';

	/* if path is root '/', return inode number 2 */
	if(strcmp("/", path) == 0)
		return EXT2_ROOT_INO;
	
	int inode_num = EXT2_ROOT_INO; /* root inode: 2 */
	inode_t inode;
	int inode_addr = 0;
	int block_size = get_block_size(&fsck_info->sblock);	
	char* filename = strtok(path, "/");
	int ret = -1;
	uint32_t pt_start_addr = fsck_info->pt.start_sec * SECT_SIZE;
	while(filename != NULL)
	{
		/* get inode addr (in byte) from inode number */
		inode_addr = compute_inode_addr(fsck_info, inode_num);

		/* read inode information from inode table entry */
		read_bytes(inode_addr, sizeof(inode_t), &inode);

		/* If this isn't a directory, print error */
		if(!EXT2_ISDIR(inode.i_mode)){
			fprintf(stderr, "ERROR: Please check if the path is a directory!\n");
			return -1;
		}
		
		int found = 0;
		uint8_t buf[block_size]; /* 1024 bytes */
		/* search in direct blocks */
		int i = 0;
		for(; i < EXT2_NDIR_BLOCKS; i++)
		{
			if (inode.i_block[i] <= 0)
				continue;
			
			read_bytes(pt_start_addr + inode.i_block[i] * block_size, 
				block_size, buf);

			/* Search the file from direct blocks */
			if((ret = search_direct_blk(fsck_info, buf, filename)) > 0){
				found = 1;
				break;
			}
		}
		/* If not found, search in indirect block */
		if(!found && inode.i_block[EXT2_IND_BLOCK] > 0)
		{
			read_bytes(pt_start_addr + inode.i_block[EXT2_IND_BLOCK] * block_size, 
			block_size, buf);
			if((ret = search_indirect_blk(fsck_info, (uint32_t*)buf, 
				filename)) > 0){
				found = 1;
			}
		}
		/* If still not found, search in d_indirect block */
		if(!found && inode.i_block[EXT2_DIND_BLOCK] > 0)
		{
			read_bytes(pt_start_addr + inode.i_block[EXT2_DIND_BLOCK] * block_size, 
			block_size, buf);
			if((ret = search_dindirect_blk(fsck_info, (uint32_t*)buf, 
				filename)) > 0){
				found = 1;
			}
		}
		/* if still not found, search in t_indirect block */
		if(!found && inode.i_block[EXT2_TIND_BLOCK] > 0)
		{
			read_bytes(pt_start_addr + inode.i_block[EXT2_TIND_BLOCK] * block_size, 
				block_size, buf);
			if((ret = search_tindirect_blk(fsck_info, (uint32_t*)buf, 
				filename)) > 0){
				found = 1;
			}
		}
		/* Cannot find the directory according to the filename */
		if(!found)
		{
			fprintf(stderr, "ERRPR: Cannot find file %s!\n", filename);
			return -1;
		}
		inode_num = ret;
		filename = strtok(NULL, "/");
	}
	return inode_num;
}

int search_direct_blk(fsck_info_t *fsck_info, 
					  uint8_t* block, 
					  char* filename)
{
	__u32 inode = 0;
	__u16 rec_len = 0;
	__u8 name_len = 0;
	__u8 type = 0;
	int block_size = get_block_size(&fsck_info->sblock);
	char name[EXT2_NAME_LEN + 1];
	int dir_entry_offset = 0;

	while(dir_entry_offset < block_size)
	{
		type = *(__u8*)(block + dir_entry_offset + FILE_TYPE_OFFSET);
		
		/* If no more directory entries in the block */
		if(type == EXT2_FT_UNKNOWN){
			return NOTFOUND;
		}
		rec_len = *(__u16*)(block + dir_entry_offset + REC_LEN_OFFSET);
		name_len = *(__u8*)(block + dir_entry_offset + NAME_LEN_OFFSET);
		memcpy(name, block + dir_entry_offset + NAME_OFFSET, name_len);
		name[name_len] = '\0';
		
		/* if find the filename, return inode number */
		if(strcmp(filename, name) == 0)
		{
			inode = *(__u32*)(block + dir_entry_offset);
			return inode;
		}
		dir_entry_offset += rec_len;
	}
	return NOTFOUND;
}

int search_indirect_blk(fsck_info_t *fsck_info,
						uint32_t* block, 
						char* filename)
{
 	int block_size = get_block_size(&fsck_info->sblock);
 	uint8_t buf[block_size];
	int inode = -1;
	int i = 0;
	uint32_t pt_start_addr = fsck_info->pt.start_sec * SECT_SIZE;

	for(; i < block_size / 4; i++)
	{
		if (block[i] == 0)
			break;

		read_bytes(pt_start_addr + block[i] * block_size, block_size, buf);
		if ((inode = search_direct_blk(fsck_info, buf, filename)) > 0)
			return inode;
	}
	return NOTFOUND;
}

int search_dindirect_blk(fsck_info_t *fsck_info,
						 uint32_t* block, 
						 char* filename)
{
	int block_size = get_block_size(&fsck_info->sblock);
 	uint32_t buf[block_size];
	int inode = -1;
	int i = 0;
	uint32_t pt_start_addr = fsck_info->pt.start_sec * SECT_SIZE;

	for(; i < block_size / 4; i++)
	{
		if (block[i] == 0)
			break;

		read_bytes(pt_start_addr + block[i] * block_size, block_size, buf);
		if ((inode = search_indirect_blk(fsck_info, buf, filename)) > 0)
			return inode;
	}
	return NOTFOUND;
}

int search_tindirect_blk(fsck_info_t *fsck_info,
						 uint32_t* block, 
						 char* filename)
{
	int block_size = get_block_size(&fsck_info->sblock);
 	uint32_t buf[block_size];
	int inode = -1;
	int i = 0;
	uint32_t pt_start_addr = fsck_info->pt.start_sec * SECT_SIZE;

	for(; i < block_size / 4; i++)
	{
		if (block[i] == 0)
			break;

		read_bytes(pt_start_addr + block[i] * block_size, block_size, buf);
		if ((inode = search_dindirect_blk(fsck_info, buf, filename)) > 0)
			return inode;
	}
	return NOTFOUND;
}


uint32_t get_dir_entry_addr(fsck_info_t *fsck_info, 
							int inode_num, 
							int entrysize)
{
	inode_t inode;
	int inode_addr = 0;
	int block_size = get_block_size(&fsck_info->sblock);
	/* Get inode addr (in byte) from inode number */
	inode_addr = compute_inode_addr(fsck_info, inode_num);
	uint32_t pt_start_addr = fsck_info->pt.start_sec * SECT_SIZE;

	/* Read inode information from inode table entry */
	read_bytes(inode_addr, sizeof(inode_t), &inode);

	/* If this isn't a directory, return */
	if(!EXT2_ISDIR(inode.i_mode))
		return -1;
 	
 	/* 1024 bytes */	
	uint8_t buf[block_size];
	/* Search in direct blocks */
	int i = 0;
	int ret = -1;
	//int found = 0;
	for(; i < EXT2_NDIR_BLOCKS; i++)
	{
		if (inode.i_block[i] <= 0)
			continue;
		
		int disk_offset = pt_start_addr + inode.i_block[i] * block_size;
		read_bytes(disk_offset, block_size, buf);
		
		/* traverse direct block[i] */
		if((ret = search_addr_in_direct_blk(fsck_info, disk_offset, buf,
		 entrysize)) > 0){
			return ret;
		}
	}
	return ret;
}

uint32_t search_addr_in_direct_blk( fsck_info_t *fsck_info,
								   	uint32_t disk_offset,
                           			uint8_t* buf,
                           			int entrysize)
{
	dir_entry_t dir_entry;
	int dir_entry_offset = 0;
	int block_size = get_block_size(&fsck_info->sblock);
	int ret = -1;

	while(1){
		dir_entry.rec_len = *(__u16*)(buf + dir_entry_offset + REC_LEN_OFFSET);
		dir_entry.name_len = *(__u8*)(buf + dir_entry_offset + NAME_LEN_OFFSET);
		dir_entry.file_type = *(__u8*)(buf + dir_entry_offset + FILE_TYPE_OFFSET);
		memcpy(dir_entry.name, buf + dir_entry_offset + NAME_OFFSET, dir_entry.name_len);
		dir_entry.name[dir_entry.name_len] = '\0';
		
		if (dir_entry_offset + dir_entry.rec_len >= block_size)
			break;

		dir_entry_offset += dir_entry.rec_len;
	}

	/* check found */
	int name_size = (dir_entry.name_len - 1) / 4 * 4 + 4;
	
	if (dir_entry_offset + NAME_OFFSET + name_size + entrysize < block_size)
	{
		ret = (disk_offset + dir_entry_offset + NAME_OFFSET + name_size);
		dir_entry.rec_len = NAME_OFFSET + name_size;
		dir_entry.inode = *(__u32*)(buf + dir_entry_offset);
		memcpy(dir_entry.name, buf + dir_entry_offset + NAME_OFFSET, dir_entry.name_len);
		dir_entry.name[dir_entry.name_len] = '\0';
		
		write_bytes(disk_offset + dir_entry_offset, 
			dir_entry.rec_len, &dir_entry);
	}
	return ret;
}
