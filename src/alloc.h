
#ifndef ALLOC_H
#define ALLOC_H

#include "xv6_fs.h"

#define BPB (BSIZE * 8)

// Allocate one free data block.
int balloc(const struct superblock *sb, uint32_t *block_out);

// Free an allocated data block.
int bfree(const struct superblock *sb, uint32_t block_no);

#endif
