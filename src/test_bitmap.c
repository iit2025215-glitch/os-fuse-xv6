#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "xv6_fs.h"
#include "disk.h"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: %s fs.img\n", argv[0]);
        return 1;
    }

    if (disk_open(argv[1], 0) < 0) {
        printf("Cannot open filesystem image\n");
        return 1;
    }

    // Read the superblock.
    unsigned char block[BSIZE];

    if (disk_read_block(1, block) < 0) {
        printf("Cannot read superblock\n");
        disk_close();
        return 1;
    }

    struct superblock sb;
    memcpy(&sb, block, sizeof(sb));

    // Read the first bitmap block.
    unsigned char bitmap[BSIZE];

    if (disk_read_block(sb.bmapstart, bitmap) < 0) {
        printf("Cannot read bitmap\n");
        disk_close();
        return 1;
    }

    printf("Bitmap block number: %u\n", sb.bmapstart);
    printf("Inspecting first 32 block allocation bits:\n\n");

printf("\n===== FREE BLOCKS =====\n");

int free_count = 0;

for (uint32_t b = 0; b < sb.size; b++) {

    int byte_index = b / 8;
    int bit_index = b % 8;

    int allocated =
        (bitmap[byte_index] >> bit_index) & 1;

    if (!allocated) {
        printf("Free block found: %u\n", b);
        free_count++;
    }
}

printf("\nTotal free blocks: %d\n", free_count);    for (int b = 64; b < 96; b++) {

        // Find which byte contains the bit.
        int byte_index = b / 8;

        // Find the position of the bit inside that byte.
        int bit_index = b % 8;

        // Extract the bit.
        int allocated =
            (bitmap[byte_index] >> bit_index) & 1;

        printf("Block %2d : %s\n",
               b,
               allocated ? "ALLOCATED" : "FREE");
    }

    disk_close();

    return 0;
}
