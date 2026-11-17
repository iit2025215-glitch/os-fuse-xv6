
#ifndef INODE_H
#define INODE_H

#include <stdint.h>
#include "xv6_fs.h"

int read_inode(
    uint32_t inum,
    const struct superblock *sb,
    struct dinode *result
);
int write_inode(
    uint32_t inum,
    const struct superblock *sb,
    const struct dinode *inode
);

int ialloc(
    const struct superblock *sb,
    uint16_t type,
    uint32_t *inum_out
);

#endif
