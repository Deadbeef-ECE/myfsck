/* @file: pass1.h
 *
 * @breif: Functions for myfsck pass1
 *
 * @author: Yuhang Jiang (yuhangj@andrew.cmu.edu)
 * @bug: No known bugs
 */

#ifndef _PASS1_H_
#define _PASS1_H_ 

void pass1_correct_dir(fsck_info_t *fsck_info);

void trav_dir(fsck_info_t *fsck_info, 
			uint32_t inode_num, uint32_t parent);

void trav_direct_blk(fsck_info_t *fsck_info, 
					int block_offset, 
					int iblock_num, 
					uint32_t current_dir, 
					uint32_t parent_dir,
					uint8_t* buf);

void trav_indirect_blk( fsck_info_t* fsck_info,
                    	uint32_t current_dir, 
                    	uint32_t parent_dir,
						uint32_t* singly_buf);

void trav_dindirect_blk(fsck_info_t* fsck_info,
                    	uint32_t current_dir, 
                    	uint32_t parent_dir,
						uint32_t* doubly_buf); 

void trav_tindirect_blk(fsck_info_t* fsck_info,
                     	uint32_t current_dir, 
                     	uint32_t parent_dir,
						uint32_t* triply_buf);



#endif /* !_PASS1_H_ */