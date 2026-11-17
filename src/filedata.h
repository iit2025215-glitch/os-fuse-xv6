
#ifndef FILEDATA_H
#define FILEDATA_H

#include <stdint.h>
#include "xv6_fs.h"

int inode_get_block(
    const struct dinode *inode,
    const struct superblock *sb,
    uint32_t logical_block,
    uint32_t *physical_block
);

int read_inode_data(
    const struct dinode *inode,
    const struct superblock *sb,
    void *buffer,
    uint32_t offset,
    uint32_t size
);
int inode_set_block(
    struct dinode *inode,
    const struct superblock *sb,
    uint32_t logical_block,
    uint32_t *physical_block
);

int write_inode_data(
    struct dinode *inode,
    const struct superblock *sb,
    const void *buffer,
    uint32_t offset,
    uint32_t size
);
#endif
