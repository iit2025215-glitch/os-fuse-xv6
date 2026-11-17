
#include <stdint.h>
#include <string.h>

#include "alloc.h"
#include "disk.h"

int balloc(const struct superblock *sb, uint32_t *block_out)
{
    unsigned char bitmap[BSIZE];
    unsigned char zero[BSIZE] = {0};

    // First data block = total blocks - data blocks.
    uint32_t data_start = sb->size - sb->nblocks;

    // Search only the data region.
    for (uint32_t b = data_start; b < sb->size; b++) {

        uint32_t bitmap_block =
            sb->bmapstart + b / BPB;

        uint32_t bit = b % BPB;

        uint32_t byte_index = bit / 8;
        uint32_t bit_index = bit % 8;

        // Read the relevant bitmap block.
        if (disk_read_block(bitmap_block, bitmap) < 0)
            return -1;

        // Check whether this block is free.
        if ((bitmap[byte_index] & (1U << bit_index)) == 0) {

            // Mark the block as allocated.
            bitmap[byte_index] |= (1U << bit_index);

            // Save the updated bitmap.
            if (disk_write_block(bitmap_block, bitmap) < 0)
                return -1;

            // Initialize the allocated block.
            if (disk_write_block(b, zero) < 0) {

                // Roll back the bitmap update on failure.
                bitmap[byte_index] &= ~(1U << bit_index);
                disk_write_block(bitmap_block, bitmap);

                return -1;
            }

            *block_out = b;
            return 0;
        }
    }

    // No free block found.
    return -1;
}

int bfree(const struct superblock *sb, uint32_t block_no)
{
    uint32_t data_start = sb->size - sb->nblocks;

    // Never allow metadata blocks to be freed.
    if (block_no < data_start || block_no >= sb->size)
        return -1;

    uint32_t bitmap_block =
        sb->bmapstart + block_no / BPB;

    uint32_t bit = block_no % BPB;

    uint32_t byte_index = bit / 8;
    uint32_t bit_index = bit % 8;

    unsigned char bitmap[BSIZE];

    if (disk_read_block(bitmap_block, bitmap) < 0)
        return -1;

    // The block must currently be allocated.
    if ((bitmap[byte_index] & (1U << bit_index)) == 0)
        return -1;

    // Mark the block as free.
    bitmap[byte_index] &= ~(1U << bit_index);

    if (disk_write_block(bitmap_block, bitmap) < 0)
        return -1;

    return 0;
}
