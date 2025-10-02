#ifndef VFS_H
#define VFS_H

#define MAX_FILES 128

typedef struct {
    char magic[4];
    uint32_t file_count;
} Superblock;

typedef struct {
    char name[32];
    uint32_t offset;
    uint32_t size;
} FileEntry;

#endif
