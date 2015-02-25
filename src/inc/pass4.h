#ifndef _PASS4_H_
#define _PASS4_H_

#define SYMLNK_SIZE			60
#define FIRST_BLK_OFFSET	1

#define CHECK(m)	(1<<((m)%8))

void pass4_fix_block_bitmap(fsck_info_t* fsck_info);
int get_last_blk_bum(int num, int blks_per_group);
uint8_t check_bit_map(fsck_info_t *fsck_info, int block_index);
uint8_t check_local_blk_map(fsck_info_t *fsck_info, int block_index);
void init_local_blkmap(fsck_info_t *fsck_info, int total);
void mark_block(fsck_info_t *fsck_info, int inode_num);
void mark_indirect_block( fsck_info_t *fsck_info, 
						  uint32_t* indirect_buf);
void mark_dindirect_block(fsck_info_t *fsck_info,
						  uint32_t* dindirect_buf);
void mark_tindirect_block(fsck_info_t *fsck_info, 
						  uint32_t* tindirect_buf);

uint8_t correct_bit_map(fsck_info_t *fsck_info, int block_index);
uint8_t correct_block_map(fsck_info_t *fsck_info, int block_index, int i);

#endif /* !_PASS4_H_ */