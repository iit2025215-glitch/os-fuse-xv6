
#include <stdio.h>
#include <string.h>

#include "disk.h"
#include "inode.h"
#include "xv6_fs.h"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: %s fs.img\n", argv[0]);
        return 1;
    }

    // Open image in read-only mode.
    if (disk_open(argv[1], 0) < 0)
        return 1;

    // Read superblock.
    char buffer[BSIZE];

    if (disk_read_block(1, buffer) < 0) {
        disk_close();
        return 1;
    }

    struct superblock sb;
    memcpy(&sb, buffer, sizeof(sb));

    if (sb.magic != FSMAGIC) {
        printf("Invalid filesystem!\n");
        disk_close();
        return 1;
    }

    // Check structure sizes.
    printf("\n===== STRUCTURE INFORMATION =====\n");

    printf("Superblock size : %zu bytes\n", sizeof(sb));
    printf("Inode size      : %zu bytes\n",
           sizeof(struct dinode));
    printf("Inodes per block: %d\n", IPB);

    // Read root inode.
    struct dinode root;

    if (read_inode(ROOTINO, &sb, &root) < 0) {
        printf("Could not read root inode!\n");
        disk_close();
        return 1;
    }

    printf("\n===== ROOT INODE INFORMATION =====\n");

    printf("Inode number : %d\n", ROOTINO);
    printf("Inode block  : %u\n", IBLOCK(ROOTINO, sb));
    printf("Inode type   : %d\n", root.type);
    printf("File size    : %u bytes\n", root.size);
    printf("Link count   : %d\n", root.nlink);

    printf("\nData block addresses:\n");

    for (int i = 0; i < NDIRECT; i++) {
        printf("Direct[%d] = %u\n", i, root.addrs[i]);
    }

    printf("Indirect    = %u\n", root.addrs[NDIRECT]);

    if (root.type == T_DIR)
        printf("\nRoot inode successfully identified as a directory!\n");
    else
        printf("\nWarning: Root inode is not a directory!\n");

    disk_close();

    return 0;
}
