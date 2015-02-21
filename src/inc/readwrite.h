#ifndef _READWRITE_H_
#define _READWRITE_H_

void print_sector (unsigned char *buf);
void read_sectors (int64_t start_sector, unsigned int num_sectors, void *into);
void write_sectors (int64_t start_sector, unsigned int num_sectors, void *from);

#endif /* !_READWRITE_H_ */