
#ifndef XV6_FS_H
#define XV6_FS_H

#include <stdint.h>

#define BSIZE 1024
#define FSMAGIC 0x10203040

#define ROOTINO 1

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint32_t))
#define MAXFILE (NDIRECT + NINDIRECT)

#define T_DIR 1
#define T_FILE 2
#define T_DEVICE 3

#define DIRSIZ 14

// Filesystem superblock
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

// On-disk inode
struct dinode {
    int16_t type;
    int16_t major;
    int16_t minor;
    int16_t nlink;

    uint32_t size;

    uint32_t addrs[NDIRECT + 1];
};

// Directory entry
struct xv6_dirent {
    uint16_t inum;
    char name[DIRSIZ];
};

// Number of inodes per block
#define IPB (BSIZE / sizeof(struct dinode))

// Calculate the disk block containing an inode
#define IBLOCK(inum, sb) \
    ((inum) / IPB + (sb).inodestart)

#endif
