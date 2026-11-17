
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define BSIZE 1024
#define FSMAGIC 0x10203040

// On-disk superblock structure.
// Must match the xv6 version used to create fs.img.
struct superblock {
    uint32_t magic;
    uint32_t size;
    uint32_t nblocks;
    uint32_t ninodes;
    uint32_t nlog;
    uint32_t logstart;
    uint32_t inodestart;
    uint32_t bmapstart;
};

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Usage: %s fs.img\n", argv[0]);
        return 1;
    }

    // Open the filesystem image.
    int fd = open(argv[1], O_RDONLY);

    if (fd < 0) {
        perror("open");
        return 1;
    }

    struct superblock sb;

    // Read the superblock from block 1.
    ssize_t bytes = pread(
        fd,
        &sb,
        sizeof(sb),
        BSIZE
    );

    if (bytes != sizeof(sb)) {
        perror("pread");
        close(fd);
        return 1;
    }

    printf("\n========== XV6 SUPERBLOCK ==========\n");

    printf("Magic       : 0x%x\n", sb.magic);
    printf("Size        : %u blocks\n", sb.size);
    printf("Data blocks : %u\n", sb.nblocks);
    printf("Inodes      : %u\n", sb.ninodes);
    printf("Log blocks  : %u\n", sb.nlog);
    printf("Log start   : %u\n", sb.logstart);
    printf("Inode start : %u\n", sb.inodestart);
    printf("Bitmap start: %u\n", sb.bmapstart);

    if (sb.magic != FSMAGIC) {
        fprintf(stderr, "\nInvalid or unexpected filesystem magic!\n");
        close(fd);
        return 1;
    }

    printf("\nFilesystem superblock read successfully!\n");

    close(fd);

    return 0;
}
