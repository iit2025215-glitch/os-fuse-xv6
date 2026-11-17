
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "xv6_fs.h"
#include "path.h"
#include "filedata.h"

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("Usage: %s fs.img /path/to/file\n", argv[0]);
        return 1;
    }

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

    // Resolve pathname.
    uint32_t inum;
    struct dinode inode;

    if (path_resolve(
            argv[2],
            &sb,
            &inum,
            &inode
        ) < 0) {

        printf("Could not resolve pathname!\n");
        disk_close();
        return 1;
    }

    printf("\n===== PATH RESOLUTION =====\n");

    printf("Path        : %s\n", argv[2]);
    printf("Inode number: %u\n", inum);
    printf("File type   : %u\n", (unsigned)inode.type);
    printf("File size   : %u bytes\n", inode.size);

    // Print file contents for regular files.
    if (inode.type == T_FILE) {

        char *content = malloc((size_t)inode.size + 1);

        if (!content) {
            disk_close();
            return 1;
        }

        int bytes = read_inode_data(
            &inode,
            &sb,
            content,
            0,
            inode.size
        );

        if (bytes < 0) {
            printf("File reading failed!\n");
            free(content);
            disk_close();
            return 1;
        }

        content[bytes] = '\0';

        printf("\n===== FILE CONTENTS =====\n\n");
        fwrite(content, 1, bytes, stdout);
        printf("\n\n===== END =====\n");

        free(content);
    }
    else if (inode.type == T_DIR) {
        printf("\nThis pathname refers to a directory.\n");
    }
    else {
        printf("\nThis pathname refers to another inode type.\n");
    }

    printf("\nPath resolution successful!\n");

    disk_close();

    return 0;
}
