
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "path.h"
#include "inode.h"
#include "filedata.h"
#include "disk.h"


// Search for a filename inside a directory.
// Return the matching inode number.
static int dir_lookup(
    const struct dinode *dir,
    const struct superblock *sb,
    const char *name,
    uint32_t *inum_out
) {
    if (dir->type != T_DIR)
        return -1;

    char *buffer = malloc(dir->size ? dir->size : 1);

    if (!buffer)
        return -1;

    int bytes = read_inode_data(
        dir,
        sb,
        buffer,
        0,
        dir->size
    );

    if (bytes < 0 || (uint32_t)bytes != dir->size) {
        free(buffer);
        return -1;
    }

    for (uint32_t offset = 0;
         offset + sizeof(struct xv6_dirent) <= dir->size;
         offset += sizeof(struct xv6_dirent))
    {
        struct xv6_dirent entry;

        memcpy(&entry, buffer + offset, sizeof(entry));

        if (entry.inum == 0)
            continue;

        char entry_name[DIRSIZ + 1];

        memcpy(entry_name, entry.name, DIRSIZ);
        entry_name[DIRSIZ] = '\0';

        if (strcmp(entry_name, name) == 0) {

            if (entry.inum >= sb->ninodes) {
                free(buffer);
                return -1;
            }

            *inum_out = entry.inum;

            free(buffer);
            return 0;
        }
    }

    free(buffer);
    return -1;
}


// Resolve an absolute pathname to its inode.
int path_resolve(
    const char *path,
    const struct superblock *sb,
    uint32_t *inum_out,
    struct dinode *inode_out
) {
    if (!path || !sb || !inum_out || !inode_out)
        return -1;

    if (path[0] != '/') {
        fprintf(stderr, "Expected an absolute path\n");
        return -1;
    }

    // Start traversal at the root directory.
    uint32_t current_inum = ROOTINO;

    struct dinode current_inode;

    if (read_inode(current_inum, sb, &current_inode) < 0)
        return -1;

    char *path_copy = malloc(strlen(path) + 1);

    if (!path_copy)
        return -1;

    strcpy(path_copy, path);

    char *saveptr = NULL;

    char *component = strtok_r(
        path_copy,
        "/",
        &saveptr
    );

    while (component != NULL) {

        if (current_inode.type != T_DIR) {
            fprintf(stderr, "A path component is not a directory\n");
            free(path_copy);
            return -1;
        }

        if (strlen(component) > DIRSIZ) {
            fprintf(stderr, "Path component is too long: %s\n", component);
            free(path_copy);
            return -1;
        }

        uint32_t next_inum;

        if (dir_lookup(
                &current_inode,
                sb,
                component,
                &next_inum
            ) < 0) {

            fprintf(stderr, "Path not found: %s\n", component);
            free(path_copy);
            return -1;
        }

        // Move to the next inode.
        current_inum = next_inum;

        if (read_inode(
                current_inum,
                sb,
                &current_inode
            ) < 0) {

            free(path_copy);
            return -1;
        }

        component = strtok_r(NULL, "/", &saveptr);
    }

    free(path_copy);

    *inum_out = current_inum;
    *inode_out = current_inode;

    return 0;
}


// Add a directory entry.
int dir_add(
    uint32_t dir_inum,
    const struct superblock *sb,
    const char *name,
    uint32_t inum
) {
    if (!name || !sb || inum == 0 || inum >= sb->ninodes)
        return -1;

    size_t name_len = strlen(name);

    if (name_len == 0 || name_len > DIRSIZ)
        return -1;

    struct dinode dir;

    if (read_inode(dir_inum, sb, &dir) < 0)
        return -1;

    if (dir.type != T_DIR)
        return -1;

    // Reject duplicate names.
    uint32_t existing_inum;

    if (dir_lookup(&dir, sb, name, &existing_inum) == 0)
        return -1;

    struct xv6_dirent entry;
    memset(&entry, 0, sizeof(entry));

    entry.inum = (uint16_t)inum;
    memcpy(entry.name, name, name_len);

    // Find an unused directory-entry slot.
    for (uint32_t offset = 0;
         offset + sizeof(entry) <= dir.size;
         offset += sizeof(entry)) {

        struct xv6_dirent current;

        if (read_inode_data(
                &dir, sb, &current, offset, sizeof(current)
            ) != sizeof(current))
            return -1;

        if (current.inum == 0) {

            uint32_t logical_block = offset / BSIZE;
            uint32_t physical_block;

            if (inode_get_block(
                    &dir, sb, logical_block, &physical_block
                ) < 0)
                return -1;

            unsigned char buffer[BSIZE];

            if (disk_read_block(physical_block, buffer) < 0)
                return -1;

            memcpy(
                buffer + (offset % BSIZE),
                &entry,
                sizeof(entry)
            );

            return disk_write_block(physical_block, buffer);
        }
    }

    return -1;
}


// Remove a directory entry.
int dir_remove(
    uint32_t dir_inum,
    const struct superblock *sb,
    const char *name
) {
    if (!name || !sb)
        return -1;

    struct dinode dir;

    if (read_inode(dir_inum, sb, &dir) < 0)
        return -1;

    if (dir.type != T_DIR)
        return -1;

    for (uint32_t offset = 0;
         offset + sizeof(struct xv6_dirent) <= dir.size;
         offset += sizeof(struct xv6_dirent)) {

        struct xv6_dirent entry;

        if (read_inode_data(
                &dir, sb, &entry, offset, sizeof(entry)
            ) != sizeof(entry))
            return -1;

        if (entry.inum == 0)
            continue;

        char entry_name[DIRSIZ + 1];

        memcpy(entry_name, entry.name, DIRSIZ);
        entry_name[DIRSIZ] = '\0';

        if (strcmp(entry_name, name) == 0) {

            uint32_t logical_block = offset / BSIZE;
            uint32_t physical_block;

            if (inode_get_block(
                    &dir, sb, logical_block, &physical_block
                ) < 0)
                return -1;

            unsigned char buffer[BSIZE];

            if (disk_read_block(physical_block, buffer) < 0)
                return -1;

            memset(
                buffer + (offset % BSIZE),
                0,
                sizeof(entry)
            );

            return disk_write_block(physical_block, buffer);
        }
    }

    return -1;
}
