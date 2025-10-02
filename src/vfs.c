
#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "vfs.h"

#define IMG_HEADER_SIZE (sizeof(Superblock) + sizeof(FileEntry) * MAX_FILES)

static FileEntry files[MAX_FILES];
static uint32_t file_count = 0;
static uint32_t img_data_end = 0;
static FILE *img_file = NULL;
static const char *img_path = NULL;

static FileEntry *find_file(const char *path) {
    if (path[0] == '/') path++;
    for (uint32_t i = 0; i < file_count; i++) {
        if (strcmp(files[i].name, path) == 0)
            return &files[i];
    }
    return NULL;
}

static int vfs_getattr(const char *path, struct stat *st, struct fuse_file_info *fi) {
    (void)(fi);
    memset(st, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
        return 0;
    }
    FileEntry *f = find_file(path);
    if (!f) return -ENOENT;
    st->st_mode = S_IFREG | 0644;
    st->st_nlink = 1;
    st->st_size = f->size;
    return 0;
}

static int vfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void)(offset);
    (void)(fi);
    (void)(flags);
    if (strcmp(path, "/") != 0)
        return -ENOENT;
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    for (uint32_t i = 0; i < file_count; i++) {
        filler(buf, files[i].name, NULL, 0, 0);
    }
    return 0;
}

static int vfs_open(const char *path, struct fuse_file_info *fi) {
    (void)(fi);
    if (!find_file(path)) return -ENOENT;
    return 0;
}

static int vfs_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi) {
    (void)(fi);
    FileEntry *f = find_file(path);
    if (!f) return -ENOENT;
    if ((uint32_t)offset >= f->size) return 0;
    if ((uint32_t)offset + size > f->size)
        size = (uint32_t)(f->size - offset);
    fseek(img_file, f->offset + offset, SEEK_SET);
    return (int)fread(buf, 1, size, img_file);
}

static void persist_metadata(void) {
    fseek(img_file, 0, SEEK_SET);
    Superblock sb = {"VFS1", file_count};
    fwrite(&sb, sizeof(sb), 1, img_file);
    fwrite(files, sizeof(FileEntry), file_count, img_file);
    fflush(img_file);
}

static int vfs_write(const char *path, const char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi) {
    (void)(fi);
    FileEntry *f = find_file(path);
    if (!f) return -ENOENT;

    uint32_t end_offset = (uint32_t)offset + (uint32_t)size;
    if (end_offset > f->size) {
        uint32_t new_offset = img_data_end;

        char *old_data = malloc(f->size);
        if (f->size > 0) {
            fseek(img_file, f->offset, SEEK_SET);
            fread(old_data, 1, f->size, img_file);
        }

        fseek(img_file, new_offset, SEEK_SET);
        fwrite(old_data, 1, f->size, img_file);
        free(old_data);

        f->offset = new_offset;
        f->size = end_offset;

        img_data_end = new_offset + f->size;

        persist_metadata();
    }
    fseek(img_file, f->offset + offset, SEEK_SET);
    fwrite(buf, 1, size, img_file);
    fflush(img_file);
    return (int)size;
}

static int vfs_unlink(const char *path) {
    if (path[0] == '/') path++;
    for (uint32_t i = 0; i < file_count; i++) {
        if (strcmp(files[i].name, path) == 0) {
            for (uint32_t j = i; j < file_count - 1; j++) {
                files[j] = files[j + 1];
            }
            file_count--;
            persist_metadata();
            return 0;
        }
    }
    return -ENOENT;
}

static int vfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void)(fi);
    (void)(mode);
    if (path[0] == '/') path++;
    if (file_count >= MAX_FILES) return -ENOSPC;
    if (find_file(path)) return -EEXIST;

    FileEntry *f = &files[file_count];
    strncpy(f->name, path, sizeof(f->name) - 1);
    f->name[sizeof(f->name) - 1] = '\0';

    uint32_t max_end = IMG_HEADER_SIZE;
    for (uint32_t i = 0; i < file_count; i++) {
        uint32_t end = files[i].offset + files[i].size;
        if (end > max_end) max_end = end;
    }
    f->offset = max_end;
    img_data_end = max_end;
    f->size = 0;
    file_count++;
    persist_metadata();
    return 0;
}

static void *vfs_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
    (void)(conn);
    cfg->kernel_cache = 1;
    img_data_end = IMG_HEADER_SIZE;
    for (uint32_t i = 0; i < file_count; i++) {
        uint32_t end = files[i].offset + files[i].size;
        if (end > img_data_end)
            img_data_end = end;
    }
    return NULL;
}

static void vfs_destroy(void *private_data) {
    (void)(private_data);
    fseek(img_file, 0, SEEK_SET);
    Superblock sb = {"VFS1", file_count};
    fwrite(&sb, sizeof(sb), 1, img_file);

    FileEntry entries[MAX_FILES] = {0};
    memcpy(entries, files, sizeof(FileEntry) * file_count);
    fwrite(entries, sizeof(FileEntry), MAX_FILES, img_file);
    fflush(img_file);

    if (img_file) {
        fclose(img_file);
        img_file = NULL;
        printf("Image saved and closed.\n");
    }
}

static struct fuse_operations vfs_oper = {
    .init     = vfs_init,
    .destroy  = vfs_destroy,
    .getattr  = vfs_getattr,
    .readdir  = vfs_readdir,
    .open     = vfs_open,
    .read     = vfs_read,
    .write    = vfs_write,
    .create   = vfs_create,
    .unlink   = vfs_unlink,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <image> <mountpoint>\n", argv[0]);
        return 1;
    }
    img_path = argv[1];
    img_file = fopen(img_path, "r+b");
    if (!img_file) {
        perror("fopen");
        return 1;
    }

    Superblock sb;
    fread(&sb, sizeof(Superblock), 1, img_file);

    if (memcmp(sb.magic, "VFS1", 4) != 0) {
        fprintf(stderr, "Error: Bad image format\n");
        fclose(img_file);
        return 1;
    }

    file_count = sb.file_count;
    if (file_count > MAX_FILES) {
        fprintf(stderr, "Error: Too many files\n");
        fclose(img_file);
        return 1;
    }
    fread(files, sizeof(FileEntry), file_count, img_file);

    argv[1] = argv[2];
    argc--;
    return fuse_main(argc, argv, &vfs_oper, NULL);
}
