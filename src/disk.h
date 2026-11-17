#ifndef DISK_H
#define DISK_H

#include <stdint.h>

#define BSIZE 1024

int disk_open(const char *path, int writable);

int disk_read_block(uint32_t block_no, void *buffer);

int disk_write_block(uint32_t block_no, const void *buffer);

uint32_t disk_block_count(void);

int disk_close(void);

#endif
