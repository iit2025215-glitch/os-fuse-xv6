#include "alloc.h"
#include "inode.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "filedata.h"
#include "disk.h"

// Find the physical disk block for a logical file block.
int inode_get_block(
    const struct dinode *inode,
    const struct superblock *sb,
    uint32_t logical_block,
    uint32_t *physical_block
) {
    if (!inode || !sb || !physical_block)
        return -1;

    // Check maximum file size.
    if (logical_block >= MAXFILE) {
        fprintf(stderr, "File block exceeds MAXFILE\n");
        return -1;
    }

    uint32_t block_no;

    // Case 1: Direct block.
    if (logical_block < NDIRECT) {
        block_no = inode->addrs[logical_block];
    }

    // Case 2: Indirect block.
    else {
        uint32_t indirect_block = inode->addrs[NDIRECT];

        // No indirect block allocated.
        if (indirect_block == 0) {
            *physical_block = 0;
            return 0;
        }

        if (indirect_block >= sb->size)
            return -1;

        // Read the indirect block.
        uint32_t addresses[NINDIRECT];

        if (disk_read_block(
                indirect_block,
                addresses
            ) < 0) {
            return -1;
        }

        uint32_t index = logical_block - NDIRECT;

        block_no = addresses[index];
    }

    // Zero means an unallocated data block.
    if (block_no == 0) {
        *physical_block = 0;
        return 0;
    }

    // Validate physical block address.
    if (block_no >= sb->size) {
        fprintf(stderr, "Invalid data block: %u\n", block_no);
        return -1;
    }

    *physical_block = block_no;

    return 0;
}


// Read file data starting at a given byte offset.
int read_inode_data(
    const struct dinode *inode,
    const struct superblock *sb,
    void *buffer,
    uint32_t offset,
    uint32_t size
) {
    if (!inode || !sb || (!buffer && size != 0))
        return -1;

    // Nothing to read beyond EOF.
    if (offset >= inode->size)
        return 0;

    // Never read beyond the file size.
    if (size > inode->size - offset)
        size = inode->size - offset;

    uint32_t total_read = 0;

    char block_buffer[BSIZE];

    while (total_read < size) {

        uint32_t current_offset = offset + total_read;

        // Find logical block number.
        uint32_t logical_block =
            current_offset / BSIZE;

        // Find position inside the block.
        uint32_t block_offset =
            current_offset % BSIZE;

        // Find physical disk block.
        uint32_t physical_block;

        if (inode_get_block(
                inode,
                sb,
                logical_block,
                &physical_block
            ) < 0) {
            return -1;
        }

        uint32_t bytes_to_read =
            BSIZE - block_offset;

        if (bytes_to_read > size - total_read)
            bytes_to_read = size - total_read;

        if (physical_block == 0) {
            // Unallocated block: return zero bytes.
            memset(
                (char *)buffer + total_read,
                0,
                bytes_to_read
            );
        }
        else {
            // Read actual data block.
            if (disk_read_block(
                    physical_block,
                    block_buffer
                ) < 0) {
                return -1;
            }

            memcpy(
                (char *)buffer + total_read,
                block_buffer + block_offset,
                bytes_to_read
            );
        }

        total_read += bytes_to_read;
    }

    return (int)total_read;
}

// Allocate or locate a physical block for an inode.
int inode_set_block(
    struct dinode *inode,
    const struct superblock *sb,
    uint32_t logical_block,
    uint32_t *physical_block
) {
    if (!inode || !sb || !physical_block || logical_block >= MAXFILE)
        return -1;

    uint32_t block;

    // Direct blocks.
    if (logical_block < NDIRECT) {
        block = inode->addrs[logical_block];

        if (block == 0) {
            if (balloc(sb, &block) < 0)
                return -1;

            inode->addrs[logical_block] = block;
        }

        *physical_block = block;
        return 0;
    }

    // Indirect block.
    uint32_t indirect = inode->addrs[NDIRECT];

    if (indirect == 0) {
        if (balloc(sb, &indirect) < 0)
            return -1;

        inode->addrs[NDIRECT] = indirect;
    }

    uint32_t addresses[NINDIRECT];

    if (disk_read_block(indirect, addresses) < 0)
        return -1;

    uint32_t index = logical_block - NDIRECT;

    if (addresses[index] == 0) {
        if (balloc(sb, &block) < 0)
            return -1;

        addresses[index] = block;

        if (disk_write_block(indirect, addresses) < 0) {
            bfree(sb, block);
            return -1;
        }
    }

    *physical_block = addresses[index];
    return 0;
}


// Write file data, allocating blocks when necessary.
int write_inode_data(
    struct dinode *inode,
    const struct superblock *sb,
    const void *buffer,
    uint32_t offset,
    uint32_t size
) {
    if (!inode || !sb || (!buffer && size != 0))
        return -1;

    if (offset > MAXFILE * BSIZE ||
        size > MAXFILE * BSIZE - offset)
        return -1;

    uint32_t total_written = 0;
    unsigned char block_buffer[BSIZE];

    while (total_written < size) {
        uint32_t current = offset + total_written;
        uint32_t logical = current / BSIZE;
        uint32_t block_offset = current % BSIZE;

        uint32_t physical;

        if (inode_set_block(inode, sb, logical, &physical) < 0)
            return total_written ? (int)total_written : -1;

        uint32_t amount = BSIZE - block_offset;

        if (amount > size - total_written)
            amount = size - total_written;

        if (disk_read_block(physical, block_buffer) < 0)
            return total_written ? (int)total_written : -1;

        memcpy(
            block_buffer + block_offset,
            (const unsigned char *)buffer + total_written,
            amount
        );

        if (disk_write_block(physical, block_buffer) < 0)
            return total_written ? (int)total_written : -1;

        total_written += amount;
    }

    if (offset + total_written > inode->size)
        inode->size = offset + total_written;

    return (int)total_written;
}
