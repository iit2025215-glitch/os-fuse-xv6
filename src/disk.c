
#include "disk.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>

static int disk_fd = -1;
static uint32_t total_blocks = 0;
static int write_enabled = 0;

// Open the filesystem image.
int disk_open(const char *path, int writable)
{
    int flags = writable ? O_RDWR : O_RDONLY;

    disk_fd = open(path, flags);

    if (disk_fd < 0) {
        perror("open disk image");
        return -1;
    }

    struct stat st;

    if (fstat(disk_fd, &st) < 0) {
        perror("fstat");
        close(disk_fd);
        disk_fd = -1;
        return -1;
    }

    if (st.st_size <= 0 || st.st_size % BSIZE != 0) {
        fprintf(stderr, "Invalid filesystem image size\n");
        close(disk_fd);
        disk_fd = -1;
        return -1;
    }

    total_blocks = st.st_size / BSIZE;
    write_enabled = writable;

    printf("Disk image opened successfully!\n");
    printf("Total blocks: %u\n", total_blocks);

    return 0;
}


// Read one complete filesystem block.
int disk_read_block(uint32_t block_no, void *buffer)
{
    if (disk_fd < 0 || buffer == NULL) {
        errno = EBADF;
        return -1;
    }

    if (block_no >= total_blocks) {
        errno = EINVAL;
        return -1;
    }

    off_t offset = (off_t)block_no * BSIZE;

    ssize_t bytes = pread(
        disk_fd,
        buffer,
        BSIZE,
        offset
    );

    if (bytes != BSIZE) {
        if (bytes >= 0)
            errno = EIO;

        perror("read block");
        return -1;
    }

    return 0;
}


// Write one complete filesystem block.
int disk_write_block(uint32_t block_no, const void *buffer)
{
    if (disk_fd < 0 || !write_enabled || buffer == NULL) {
        errno = EBADF;
        return -1;
    }

    if (block_no >= total_blocks) {
        errno = EINVAL;
        return -1;
    }

    off_t offset = (off_t)block_no * BSIZE;

    ssize_t bytes = pwrite(
        disk_fd,
        buffer,
        BSIZE,
        offset
    );

    if (bytes != BSIZE) {
        if (bytes >= 0)
            errno = EIO;

        perror("write block");
        return -1;
    }

    return 0;
}


// Return total number of blocks.
uint32_t disk_block_count(void)
{
    return total_blocks;
}


// Close the filesystem image.
int disk_close(void)
{
    if (disk_fd < 0)
        return -1;

    int result = close(disk_fd);

    disk_fd = -1;
    total_blocks = 0;
    write_enabled = 0;

    return result;
}
