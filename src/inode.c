
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "inode.h"
#include "disk.h"

// Read an inode from the filesystem image.
int read_inode(
    uint32_t inum,
    const struct superblock *sb,
    struct dinode *result
) {
    // Validate inode number.
    if (inum >= sb->ninodes) {
        fprintf(stderr, "Invalid inode number: %u\n", inum);
        return -1;
    }

    // Find the disk block containing this inode.
    uint32_t block_no = IBLOCK(inum, (*sb));

    // Temporary block buffer.
    char buffer[BSIZE];

    // Read the inode block.
    if (disk_read_block(block_no, buffer) < 0) {
        return -1;
    }

    // Find the inode's position within the block.
    uint32_t index = inum % IPB;

    // Calculate its byte offset.
    uint32_t offset = index * sizeof(struct dinode);

    // Copy inode data into the output structure.
    memcpy(result, buffer + offset, sizeof(struct dinode));

    return 0;
}
// Write a modified inode back to the filesystem image.
int write_inode(
    uint32_t inum,
    const struct superblock *sb,
    const struct dinode *inode
) {
    if (inum >= sb->ninodes)
        return -1;

    uint32_t block_no = IBLOCK(inum, (*sb));

    char buffer[BSIZE];

    if (disk_read_block(block_no, buffer) < 0)
        return -1;

    uint32_t index = inum % IPB;
    uint32_t offset = index * sizeof(struct dinode);

    memcpy(buffer + offset, inode, sizeof(struct dinode));

    if (disk_write_block(block_no, buffer) < 0)
        return -1;

    return 0;
}


// Allocate a free inode.
int ialloc(
    const struct superblock *sb,
    uint16_t type,
    uint32_t *inum_out
) {
    if (type != T_DIR && type != T_FILE && type != T_DEVICE)
        return -1;

    for (uint32_t inum = 1; inum < sb->ninodes; inum++) {

        struct dinode inode;

        if (read_inode(inum, sb, &inode) < 0)
            return -1;

        // An inode with type 0 is free.
        if (inode.type == 0) {

            memset(&inode, 0, sizeof(inode));

            inode.type = type;
            inode.nlink = 1;

            if (write_inode(inum, sb, &inode) < 0)
                return -1;

            *inum_out = inum;

            return 0;
        }
    }

    return -1;
}
