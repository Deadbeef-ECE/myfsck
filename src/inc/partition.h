#ifndef _PARTITION_H_
#define _PARTITION_H_

#define PT_OFFSET	0x1be
#define PT_ENTRY_SZ 16

/** Different partition types */
#define EXT_2		0x83
#define UNUSED		0x00
#define EXTEND		0x05
#define LINUX_SWAP	0x82

typedef struct partition{
	uint32_t pt_num;
	uint32_t type;
	uint32_t start_sec;
	uint32_t length;
}partition_t;




#endif /* !_PARTITION_H_ */