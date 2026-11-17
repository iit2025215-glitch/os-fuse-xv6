#include <stdio.h>
#include <string.h>

#include "xv6_fs.h"
#include "disk.h"
#include "path.h"

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

    // Add test.txt to the root directory.
    if (dir_add(ROOTINO, &sb, "test.txt", 28) < 0) {
        printf("Directory insertion failed!\n");
        disk_close();
        return 1;
    }

    printf("Directory entry added successfully!\n");

    // Check whether path resolution can find it.
    uint32_t inum;
    struct dinode inode;

    if (path_resolve("/test.txt", &sb, &inum, &inode) == 0)
        printf("Path resolved! Inode: %u\n", inum);
    else
        printf("Path resolution failed!\n");

    // Remove the directory entry.
    if (dir_remove(ROOTINO, &sb, "test.txt") < 0) {
        printf("Directory removal failed!\n");
        disk_close();
        return 1;
    }

    printf("Directory entry removed successfully!\n");

    // Confirm that it can no longer be found.
    if (path_resolve("/test.txt", &sb, &inum, &inode) < 0)
        printf("Path no longer exists!\n");
    else
        printf("ERROR: Path still exists!\n");

    disk_close();

    return 0;
}
