
#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

#include "xv6_fs.h"
#include "disk.h"
#include "inode.h"
#include "path.h"
#include "filedata.h"
#include "alloc.h"

static struct superblock sb;

/* Get file attributes */
static int xv6_getattr(const char *path, struct stat *st,
                       struct fuse_file_info *fi)
{
    (void)fi;

    uint32_t inum;
    struct dinode inode;

    memset(st, 0, sizeof(*st));

    if (path_resolve(path, &sb, &inum, &inode) < 0)
        return -ENOENT;

    st->st_ino = inum;
    st->st_nlink = inode.nlink;
    st->st_size = inode.size;
    st->st_blksize = BSIZE;
    st->st_blocks = (inode.size + 511) / 512;

    if (inode.type == T_DIR)
        st->st_mode = S_IFDIR | 0755;
    else if (inode.type == T_FILE)
        st->st_mode = S_IFREG | 0644;
    else if (inode.type == T_DEVICE)
        st->st_mode = S_IFCHR | 0444;
    else
        return -ENOENT;

    return 0;
}

/* Read directory contents */
static int xv6_readdir(const char *path, void *buf,
                       fuse_fill_dir_t filler, off_t offset,
                       struct fuse_file_info *fi,
                       enum fuse_readdir_flags flags)
{
    (void)fi;
    (void)flags;

    uint32_t inum;
    struct dinode inode;

    if (path_resolve(path, &sb, &inum, &inode) < 0)
        return -ENOENT;

    if (inode.type != T_DIR)
        return -ENOTDIR;

    char *data = malloc(inode.size ? inode.size : 1);
    if (!data)
        return -ENOMEM;

    int bytes_read = read_inode_data(
        &inode, &sb, data, 0, inode.size
    );

    if (bytes_read < 0) {
        free(data);
        return -EIO;
    }

    for (int pos = 0;
         pos + (int)sizeof(struct xv6_dirent) <= bytes_read;
         pos += sizeof(struct xv6_dirent)) {

        struct xv6_dirent entry;
        memcpy(&entry, data + pos, sizeof(entry));

        if (entry.inum == 0)
            continue;

        off_t next_offset =
            pos / sizeof(struct xv6_dirent) + 1;

        if (next_offset <= offset)
            continue;

        char name[DIRSIZ + 1];
        memcpy(name, entry.name, DIRSIZ);
        name[DIRSIZ] = '\0';

        if (filler(buf, name, NULL, next_offset, 0))
            break;
    }

    free(data);
    return 0;
}

/* Read file contents */
static int xv6_read(const char *path, char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi)
{
    (void)fi;

    uint32_t inum;
    struct dinode inode;

    if (path_resolve(path, &sb, &inum, &inode) < 0)
        return -ENOENT;

    if (inode.type != T_FILE)
        return -EISDIR;

    if (offset < 0)
        return -EINVAL;

    if ((uint64_t)offset >= inode.size)
        return 0;

    uint32_t available = inode.size - (uint32_t)offset;
    uint32_t amount =
        size > available ? available : (uint32_t)size;

    int result = read_inode_data(
        &inode, &sb, buf, (uint32_t)offset, amount
    );

    return result < 0 ? -EIO : result;
}

/* Write file contents */
static int xv6_write(const char *path, const char *buf,
                     size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
    (void)fi;

    if (offset < 0 || (uint64_t)offset > UINT32_MAX ||
        size > UINT32_MAX)
        return -EINVAL;

    if (size > UINT32_MAX - (uint32_t)offset)
        return -EFBIG;

    uint32_t inum;
    struct dinode inode;

    if (path_resolve(path, &sb, &inum, &inode) < 0)
        return -ENOENT;

    if (inode.type != T_FILE)
        return -EISDIR;

    int written = write_inode_data(
        &inode, &sb, buf, (uint32_t)offset, (uint32_t)size
    );

    if (written < 0)
        return -EIO;

    if (write_inode(inum, &sb, &inode) < 0)
        return -EIO;

    return written;
}

/* Create a regular file */
static int xv6_create(const char *path, mode_t mode,
                      struct fuse_file_info *fi)
{
    (void)mode;
    (void)fi;

    if (!path || path[0] != '/')
        return -EINVAL;

    char copy[4096];
    if (strlen(path) >= sizeof(copy))
        return -ENAMETOOLONG;

    strcpy(copy, path);

    char *slash = strrchr(copy, '/');

    if (!slash || slash[1] == '\0')
        return -EINVAL;

    char name[DIRSIZ + 1];

    if (strlen(slash + 1) > DIRSIZ ||
        strcmp(slash + 1, ".") == 0 ||
        strcmp(slash + 1, "..") == 0)
        return -EINVAL;

    strcpy(name, slash + 1);

    char parent_path[4096];

    if (slash == copy)
        strcpy(parent_path, "/");
    else {
        *slash = '\0';
        strcpy(parent_path, copy);
    }

    uint32_t parent_inum;
    struct dinode parent;

    if (path_resolve(parent_path, &sb,
                     &parent_inum, &parent) < 0)
        return -ENOENT;

    if (parent.type != T_DIR)
        return -ENOTDIR;

    uint32_t existing_inum;
    struct dinode existing;

    if (path_resolve(path, &sb, &existing_inum, &existing) == 0)
        return -EEXIST;

    uint32_t new_inum;

    if (ialloc(&sb, T_FILE, &new_inum) < 0)
        return -ENOSPC;

    if (dir_add(parent_inum, &sb, name, new_inum) < 0) {
        struct dinode empty = {0};
        write_inode(new_inum, &sb, &empty);
        return -ENOSPC;
    }

    return 0;
}

/* Create a directory */
static int xv6_mkdir(const char *path, mode_t mode)
{
    (void)mode;

    if (!path || strlen(path) >= 4096)
        return -ENAMETOOLONG;

    char copy[4096];
    char parent_path[4096];
    char name[DIRSIZ + 1];

    strcpy(copy, path);

    char *slash = strrchr(copy, '/');

    if (!slash || slash[1] == '\0')
        return -EINVAL;

    if (strlen(slash + 1) > DIRSIZ ||
        strcmp(slash + 1, ".") == 0 ||
        strcmp(slash + 1, "..") == 0)
        return -EINVAL;

    strcpy(name, slash + 1);

    if (slash == copy)
        strcpy(parent_path, "/");
    else {
        *slash = '\0';
        strcpy(parent_path, copy);
    }

    uint32_t parent_inum;
    struct dinode parent;

    if (path_resolve(parent_path, &sb,
                     &parent_inum, &parent) < 0)
        return -ENOENT;

    if (parent.type != T_DIR)
        return -ENOTDIR;

    uint32_t existing_inum;
    struct dinode existing;

    if (path_resolve(path, &sb, &existing_inum, &existing) == 0)
        return -EEXIST;

    uint32_t new_inum;

    if (ialloc(&sb, T_DIR, &new_inum) < 0)
        return -ENOSPC;

    uint32_t block;

    if (balloc(&sb, &block) < 0) {
        struct dinode empty = {0};
        write_inode(new_inum, &sb, &empty);
        return -ENOSPC;
    }

    struct dinode new_dir = {0};
    new_dir.type = T_DIR;
    new_dir.nlink = 1;
    new_dir.size = 2 * sizeof(struct xv6_dirent);
    new_dir.addrs[0] = block;

    char buffer[BSIZE] = {0};
    struct xv6_dirent *entries =
        (struct xv6_dirent *)buffer;

    entries[0].inum = new_inum;
    strcpy(entries[0].name, ".");

    entries[1].inum = parent_inum;
    strcpy(entries[1].name, "..");

    if (disk_write_block(block, buffer) < 0 ||
        write_inode(new_inum, &sb, &new_dir) < 0) {
        bfree(&sb, block);
        struct dinode empty = {0};
        write_inode(new_inum, &sb, &empty);
        return -EIO;
    }

    if (dir_add(parent_inum, &sb, name, new_inum) < 0) {
        bfree(&sb, block);
        struct dinode empty = {0};
        write_inode(new_inum, &sb, &empty);
        return -ENOSPC;
    }

    return 0;
}

/* Free all data blocks belonging to an inode */
static int free_inode_blocks(struct dinode *inode)
{
    for (uint32_t i = 0; i < NDIRECT; i++) {
        uint32_t block = inode->addrs[i];

        if (block != 0) {
            if (bfree(&sb, block) < 0)
                return -EIO;

            inode->addrs[i] = 0;
        }
    }

    uint32_t indirect = inode->addrs[NDIRECT];

    if (indirect != 0) {
        uint32_t addresses[NINDIRECT];

        if (disk_read_block(indirect, addresses) < 0)
            return -EIO;

        for (uint32_t i = 0; i < NINDIRECT; i++) {
            if (addresses[i] != 0) {
                if (bfree(&sb, addresses[i]) < 0)
                    return -EIO;
            }
        }

        if (bfree(&sb, indirect) < 0)
            return -EIO;

        inode->addrs[NDIRECT] = 0;
    }

    return 0;
}

/* Remove a regular file */
static int xv6_unlink(const char *path)
{
    if (!path || strlen(path) >= 4096)
        return -EINVAL;

    char copy[4096];
    strcpy(copy, path);

    char *slash = strrchr(copy, '/');

    if (!slash || slash[1] == '\0')
        return -EINVAL;

    char name[DIRSIZ + 1];

    if (strlen(slash + 1) > DIRSIZ)
        return -ENAMETOOLONG;

    strcpy(name, slash + 1);

    char parent_path[4096];

    if (slash == copy)
        strcpy(parent_path, "/");
    else {
        *slash = '\0';
        strcpy(parent_path, copy);
    }

    uint32_t parent_inum, inum;
    struct dinode parent, inode;

    if (path_resolve(parent_path, &sb,
                     &parent_inum, &parent) < 0)
        return -ENOENT;

    if (path_resolve(path, &sb, &inum, &inode) < 0)
        return -ENOENT;

    if (inode.type == T_DIR)
        return -EISDIR;

    if (inode.type != T_FILE)
        return -EINVAL;

    if (dir_remove(parent_inum, &sb, name) < 0)
        return -EIO;

    if (free_inode_blocks(&inode) < 0)
        return -EIO;

    memset(&inode, 0, sizeof(inode));

    if (write_inode(inum, &sb, &inode) < 0)
        return -EIO;

    return 0;
}

/* Change the size of a regular file - FUSE3 signature */
static int xv6_truncate(const char *path, off_t new_size,
                        struct fuse_file_info *fi)
{
    (void)fi;

    if (new_size < 0 ||
        (uint64_t)new_size > (uint64_t)MAXFILE * BSIZE)
        return -EINVAL;

    uint32_t inum;
    struct dinode inode;

    if (path_resolve(path, &sb, &inum, &inode) < 0)
        return -ENOENT;

    if (inode.type != T_FILE)
        return -EISDIR;

    uint32_t old_size = inode.size;
    uint32_t target = (uint32_t)new_size;

    if (target < old_size) {
        uint32_t first_free = (target + BSIZE - 1) / BSIZE;
        uint32_t old_blocks = (old_size + BSIZE - 1) / BSIZE;

        for (uint32_t i = first_free; i < old_blocks; i++) {
            uint32_t physical;

            if (inode_get_block(&inode, &sb, i, &physical) < 0)
                return -EIO;

            if (physical == 0)
                continue;

            if (bfree(&sb, physical) < 0)
                return -EIO;

            if (i < NDIRECT) {
                inode.addrs[i] = 0;
            } else {
                uint32_t indirect = inode.addrs[NDIRECT];
                uint32_t addresses[NINDIRECT];

                if (indirect == 0 ||
                    disk_read_block(indirect, addresses) < 0)
                    return -EIO;

                addresses[i - NDIRECT] = 0;

                if (disk_write_block(indirect, addresses) < 0)
                    return -EIO;
            }
        }

        /* Release indirect block if empty */
        if (inode.addrs[NDIRECT] != 0) {
            uint32_t addresses[NINDIRECT];
            uint32_t indirect = inode.addrs[NDIRECT];
            int empty = 1;

            if (disk_read_block(indirect, addresses) < 0)
                return -EIO;

            for (uint32_t i = 0; i < NINDIRECT; i++) {
                if (addresses[i] != 0) {
                    empty = 0;
                    break;
                }
            }

            if (empty) {
                if (bfree(&sb, indirect) < 0)
                    return -EIO;

                inode.addrs[NDIRECT] = 0;
            }
        }

        /* Zero bytes after new EOF in retained block */
        if (target % BSIZE != 0) {
            uint32_t physical;

            if (inode_get_block(&inode, &sb,
                                target / BSIZE, &physical) < 0)
                return -EIO;

            if (physical != 0) {
                unsigned char buffer[BSIZE];

                if (disk_read_block(physical, buffer) < 0)
                    return -EIO;

                memset(buffer + target % BSIZE, 0,
                       BSIZE - target % BSIZE);

                if (disk_write_block(physical, buffer) < 0)
                    return -EIO;
            }
        }
    }

    /* Growing creates sparse, zero-filled space */
    inode.size = target;

    if (write_inode(inum, &sb, &inode) < 0)
        return -EIO;

    return 0;
}

/* Register all eight FUSE operations */
static const struct fuse_operations operations = {
    .getattr  = xv6_getattr,
    .readdir  = xv6_readdir,
    .read     = xv6_read,
    .write    = xv6_write,
    .create   = xv6_create,
    .mkdir    = xv6_mkdir,
    .unlink   = xv6_unlink,
    .truncate = xv6_truncate,
};

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr,
                "Usage: %s fs.img mountpoint [options]\n",
                argv[0]);
        return 1;
    }

    if (disk_open(argv[1], 1) < 0) {
        fprintf(stderr, "Cannot open xv6 image\n");
        return 1;
    }

    unsigned char block[BSIZE];

    if (disk_read_block(1, block) < 0) {
        fprintf(stderr, "Cannot read superblock\n");
        disk_close();
        return 1;
    }

    memcpy(&sb, block, sizeof(sb));

    if (sb.magic != FSMAGIC) {
        fprintf(stderr, "Invalid xv6 filesystem image\n");
        disk_close();
        return 1;
    }

    printf("xv6 filesystem image opened successfully!\n");
    printf("Total blocks: %u\n", sb.size);

    int fuse_argc = argc - 1;
    char **fuse_argv = calloc(fuse_argc, sizeof(char *));

    if (!fuse_argv) {
        disk_close();
        return 1;
    }

    fuse_argv[0] = argv[0];

    for (int i = 1; i < fuse_argc; i++)
        fuse_argv[i] = argv[i + 1];

    int result = fuse_main(
        fuse_argc, fuse_argv, &operations, NULL
    );

    free(fuse_argv);
    disk_close();

    return result;
}
