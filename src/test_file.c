
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "inode.h"
#include "filedata.h"
#include "xv6_fs.h"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: %s fs.img\n", argv[0]);
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

    // README has inode number 2 in our current image.
    struct dinode file;

    if (read_inode(2, &sb, &file) < 0) {
        disk_close();
        return 1;
    }

    if (file.type != T_FILE) {
        printf("Inode 2 is not a regular file!\n");
        disk_close();
        return 1;
    }

    printf("\n===== READING README =====\n");

    printf("Inode number : 2\n");
    printf("File size    : %u bytes\n", file.size);

    // Allocate enough memory for the complete file.
    char *content = malloc((size_t)file.size + 1);

    if (!content) {
        perror("malloc");
        disk_close();
        return 1;
    }

    // Read actual file contents.
    int bytes = read_inode_data(
        &file,
        &sb,
        content,
        0,
        file.size
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

    printf("\n\n===== END OF FILE =====\n");

    printf("Successfully read %d bytes!\n", bytes);

    free(content);
    disk_close();

    return 0;
}
