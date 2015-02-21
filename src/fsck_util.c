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

extern int device;

int parse_pt_info(partition_t *pt, uint32_t pt_num){
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
	read_sectors(0, 1, buf);
	

	if(pt_num <= PT_E_NUM){
		pt_entry_offset = PT_OFFSET + (pt_num - 1) * PT_ENTRY_SZ;
		pt->type = buf[pt_entry_offset + TYPE_OFFSET];
		pt->sec_addr = *(uint32_t*)(buf+ pt_entry_offset+ SECT_OFFSET);
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
		read_sectors(cur_ebr_sect, 1, buf);

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

	printf("cur_ebr_sect:%d, first_ebr_sect:%d\n", 
		(int)cur_ebr_sect, (int) first_ebr_sect);

	read_sectors(cur_ebr_sect, 1, buf);
	print_sector(buf);
	pt->type = buf[PT_OFFSET + TYPE_OFFSET];
	pt->sec_addr = cur_ebr_sect + *(uint32_t*)(buf+ PT_OFFSET+ SECT_OFFSET);
	pt->length =  *(uint32_t*)(buf+ PT_OFFSET+ LEN_OFFSET);
	
	return PARSE_SUCC;
}

void print_pt_info(partition_t *pt){
	fprintf(stdout, "0x%02X %d %d\n", 
		pt->type, (int)pt->sec_addr, (int)pt->length);
}

// void debug_sbock(partition_t pt, sblock_t sb){
// 	fprintf(stdout, "************ partition[%d] *************\n", 
// 		pt.pt_num);
// 	printf("start sector = %d  base = %d\n", pt.sec_addr, 
// 		pt.sec_addr*512);
// 	printf("block size = %d\n", sb.block_size);
// 	printf("inode size = %d\n\n", sb.inode_size);
// 	printf("number of blocks = %d\n", sb.num_blocks);
// 	printf("blocks per group = %d\n\n", sb.blocks_per_group);
// 	printf("number of inodes = %d\n", sb.num_inodes);
// 	printf("inodes per group = %d\n\n", sb.inodes_per_group);
// 	printf("number of groups = %d\n", sb.num_groups);
// 	printf("**************************************\n\n");
// 	return;
// }

int fsck_info_init(int pt_num)
{
	if (read_partition_info(pt_num, &pt_info) == -1)
	{	
		printf("read superblock info of partition %d failed\n", 
		        partition_num);
		return -1;
	}
	// if (read_superblock_info(pt_num) == -1)
	// {
	// 	printf("read superblock info of partition %d failed\n", 
	// 	        partition_num);
	// 	return -1;
	// }
	// if ((read_bg_desc_table(pt_num)) == -1)
	// {
	// 	printf("read bg descriptor table of partition %d failed\n",
	// 	        partition_num);
	// 	return -1;
	// }
	return 0;
}
