#include <stdio.h>
#include <string.h>

#include "xv6_fs.h"
#include "disk.h"
#include "alloc.h"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: %s fs.img\n", argv[0]);
        return 1;
    }

    // Open image in writable mode.
    if (disk_open(argv[1], 1) < 0)
        return 1;

    unsigned char block[BSIZE];
    struct superblock sb;

    if (disk_read_block(1, block) < 0) {
        disk_close();
        return 1;
    }

    memcpy(&sb, block, sizeof(sb));

    uint32_t allocated_block;

    // Test allocation.
    if (balloc(&sb, &allocated_block) < 0) {
        printf("Allocation failed!\n");
        disk_close();
        return 1;
    }

    printf("Allocated block: %u\n", allocated_block);

    // Test deallocation.
    if (bfree(&sb, allocated_block) < 0) {
        printf("Freeing failed!\n");
        disk_close();
        return 1;
    }

    printf("Block %u successfully freed!\n", allocated_block);

    disk_close();
    return 0;
}
