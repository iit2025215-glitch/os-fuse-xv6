
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: %s fs.img\n", argv[0]);
        return 1;
    }

    // Open image in READ-ONLY mode.
    if (disk_open(argv[1], 0) < 0)
        return 1;

    char buffer[BSIZE];

    // Read block 1 (superblock).
    if (disk_read_block(1, buffer) < 0) {
        disk_close();
        return 1;
    }

    uint32_t magic;

    memcpy(&magic, buffer, sizeof(magic));

    printf("\n===== DISK BLOCK TEST =====\n");

    printf("Block number : 1\n");
    printf("Block size   : %d bytes\n", BSIZE);
    printf("Magic number : 0x%x\n", magic);

    if (magic == 0x10203040) {
        printf("\nBlock reading successful!\n");
    } else {
        printf("\nUnexpected magic number!\n");
    }

    disk_close();

    return 0;
}
