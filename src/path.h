
#ifndef PATH_H
#define PATH_H

#include <stdint.h>
#include "xv6_fs.h"

int path_resolve(
    const char *path,
    const struct superblock *sb,
    uint32_t *inum_out,
    struct dinode *inode_out
);
int dir_add(
    uint32_t dir_inum,
    const struct superblock *sb,
    const char *name,
    uint32_t inum
);

int dir_remove(
    uint32_t dir_inum,
    const struct superblock *sb,
    const char *name
);
#endif
