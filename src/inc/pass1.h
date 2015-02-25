#ifndef _PASS1_H_
#define _PASS1_H_ 
void pass1_correct_dir(fsck_info_t *fsck_info, 
			uint32_t inode_num, uint32_t parent);
void trav_dir(fsck_info_t *fsck_info, 
			uint32_t inode_num, uint32_t parent);

void trav_direct_blk(fsck_info_t *fsck_info, 
					int block_offset, 
					int iblock_num, 
					uint8_t* buf, 
					uint32_t current_dir, 
					uint32_t parent_dir);

void trav_indirect_blk( fsck_info_t* fsck_info,
						uint32_t* singly_buf, 
                    	uint32_t current_dir, 
                    	uint32_t parent_dir);
void trav_dindirect_blk(fsck_info_t* fsck_info,
						uint32_t* doubly_buf, 
                    	uint32_t current_dir, 
                    	uint32_t parent_dir);
void trav_tindirect_blk(fsck_info_t* fsck_info,
						uint32_t* triply_buf, 
                     	uint32_t current_dir, 
                     	uint32_t parent_dir);



#endif /* !_PASS1_H_ */