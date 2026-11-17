
#include <stdio.h>
#include <stdint.h>
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

    // Open filesystem image in read-only mode.
    if (disk_open(argv[1], 0) < 0)
        return 1;

    // Read superblock.
    char sb_buffer[BSIZE];

    if (disk_read_block(1, sb_buffer) < 0) {
        disk_close();
        return 1;
    }

    struct superblock sb;
    memcpy(&sb, sb_buffer, sizeof(sb));

    if (sb.magic != FSMAGIC) {
        printf("Invalid filesystem!\n");
        disk_close();
        return 1;
    }

    // Read root inode.
    struct dinode root;

    if (read_inode(ROOTINO, &sb, &root) < 0) {
        disk_close();
        return 1;
    }

    if (root.type != T_DIR) {
        printf("Root inode is not a directory!\n");
        disk_close();
        return 1;
    }

    printf("\n===== XV6 ROOT DIRECTORY =====\n\n");

    printf("Root inode       : %d\n", ROOTINO);
    printf("Directory size   : %u bytes\n", root.size);
    printf("Directory blocks : %u\n\n",
           (root.size + BSIZE - 1) / BSIZE);

    // Read each directory data block.
    for (uint32_t offset = 0;
         offset < root.size;
         offset += BSIZE)
    {
        uint32_t file_block_index = offset / BSIZE;

        // This initial version supports direct blocks only.
        if (file_block_index >= NDIRECT) {
            printf("Directory requires indirect block support.\n");
            disk_close();
            return 1;
        }

        uint32_t block_no = root.addrs[file_block_index];

        if (block_no == 0) {
            printf("Unexpected missing directory block!\n");
            disk_close();
            return 1;
        }

        char buffer[BSIZE];

        if (disk_read_block(block_no, buffer) < 0) {
            disk_close();
            return 1;
        }

        uint32_t valid_bytes = root.size - offset;

        if (valid_bytes > BSIZE)
            valid_bytes = BSIZE;

        // Extract directory entries.
        for (uint32_t pos = 0;
             pos + sizeof(struct xv6_dirent) <= valid_bytes;
             pos += sizeof(struct xv6_dirent))
        {
            struct xv6_dirent entry;

            memcpy(&entry, buffer + pos, sizeof(entry));

            // Inode number 0 means unused entry.
            if (entry.inum == 0)
                continue;

            // Directory names are fixed-width, not necessarily
            // null-terminated.
            char name[DIRSIZ + 1];

            memcpy(name, entry.name, DIRSIZ);
            name[DIRSIZ] = '\0';

            printf("Name: %-14s | Inode: %u\n",
                   name, entry.inum);
        }
    }

    printf("\nDirectory parsing completed successfully!\n");

    disk_close();

    return 0;
}
