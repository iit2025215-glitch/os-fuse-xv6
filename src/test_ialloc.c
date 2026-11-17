#include <stdio.h>
#include <string.h>

#include "xv6_fs.h"
#include "disk.h"
#include "inode.h"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: %s fs.img\n", argv[0]);
        return 1;
    }

    if (disk_open(argv[1], 1) < 0)
        return 1;

    unsigned char buffer[BSIZE];
    struct superblock sb;

    if (disk_read_block(1, buffer) < 0) {
        disk_close();
        return 1;
    }

    memcpy(&sb, buffer, sizeof(sb));

    uint32_t new_inum;

    if (ialloc(&sb, T_FILE, &new_inum) < 0) {
        printf("Inode allocation failed!\n");
        disk_close();
        return 1;
    }

    printf("Allocated inode: %u\n", new_inum);

    struct dinode inode;

    if (read_inode(new_inum, &sb, &inode) == 0) {
        printf("Inode type: %d\n", inode.type);
        printf("Inode link count: %d\n", inode.nlink);
        printf("Inode size: %u\n", inode.size);
    }

    disk_close();

    return 0;
}
