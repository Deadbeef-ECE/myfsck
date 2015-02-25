/* @file: pass2.h
 *
 * @breif: Functions for myfsck pass2
 *
 * @author: Yuhang Jiang (yuhangj@andrew.cmu.edu)
 * @bug: No known bugs
 */

#ifndef _PASS2_H_
#define _PASS2_H_

#define NOTFOUND	-1

void pass2_fix_unref_inode(fsck_info_t *fsck_info);
int get_parent_inode(fsck_info_t *fsck_info, inode_t* inode);
int gen_filetype(__u16 i_mode);
int get_inode(fsck_info_t *fsck_info, const char* filepath);
int add_lostfound(fsck_info_t *fsck_info, int inode_num);

int search_direct_blk(fsck_info_t *fsck_info, 
					  uint8_t* block, 
					  char* filename);

int search_indirect_blk(fsck_info_t *fsck_info,
						uint32_t* block, 
						char* filename);

int search_dindirect_blk(fsck_info_t *fsck_info,
						 uint32_t* block, 
						 char* filename);

int search_tindirect_blk(fsck_info_t *fsck_info,
						 uint32_t* block, 
						 char* filename);

uint32_t get_dir_entry_addr(fsck_info_t *fsck_info, 
							int inode_num, 
							int entrysize);

uint32_t search_addr_in_direct_blk( fsck_info_t *fsck_info,
								   	uint32_t disk_offset,
                           			uint8_t* buf,
                           			int entrysize);

#endif /* !_PASS2_H_ */