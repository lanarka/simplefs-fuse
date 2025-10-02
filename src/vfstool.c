
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>

#include "vfs.h"

int compare(const void* a, const void* b) {
    const FileEntry *fa = (const FileEntry *)a;
    const FileEntry *fb = (const FileEntry *)b;
    if (fa->offset < fb->offset) return -1;
    if (fa->offset > fb->offset) return 1;
    return 0;
}

void write_padding(FILE* f, size_t count) {
    char zero = 0;
    for (size_t i = 0; i < count; i++) {
        fwrite(&zero, 1, 1, f);
    }
}

int fsck(const char* filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("fopen");
        return 1;
    }

    Superblock sb;
    if (fread(&sb, sizeof(sb), 1, f) != 1) {
        fprintf(stderr, "Failed to read superblock\n");
        fclose(f);
        return 1;
    }

    if (memcmp(sb.magic, "VFS1", 4) != 0) {
        fprintf(stderr, "Bad magic\n");
        fclose(f);
        return 1;
    }

    if (sb.file_count > MAX_FILES) {
        fprintf(stderr, "Too many files\n");
        fclose(f);
        return 1;
    }

    FileEntry files[MAX_FILES];
    if (fread(files, sizeof(FileEntry), sb.file_count, f) != sb.file_count) {
        fprintf(stderr, "Failed to read file entries\n");
        fclose(f);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long img_size = ftell(f);

    qsort(files, sb.file_count, sizeof(FileEntry), compare);
    printf("Check %s:\n", filename);

    for (uint32_t i = 0; i < sb.file_count; i++) {
        FileEntry *curr = &files[i];
        uint32_t end = curr->offset + curr->size;
        printf("  [%02u] %-32s offset=%u size=%u \n", i, curr->name, curr->offset, curr->size);

        if ((long)end > img_size)  printf("    Error: File exceeds image size (%ld bytes)\n", img_size);
        
        if (i > 0) {
            FileEntry *prev = &files[i - 1];
            uint32_t prev_end = prev->offset + prev->size;
            if (curr->offset < prev_end)
                printf("    Collision: overlaps with previous file '%s'\n", prev->name);
        }
    }

    fclose(f);
    return 0;
}

void showhelp(const char* program) {
    printf("usage: %s <subcommand> <image> [<files> ...]\n\n", program);
    printf("Subcommands:\n");
    printf(" mkfs <image> <file1> [<file2> ...]  Make new disk image\n");
    printf(" fsck <image>                        Check disk\n");
    printf(" -h                                  Show this help\n\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Missing options. Use -h for help.\n");
        return 1;
    }

    if (strcmp(argv[1], "-h") == 0) {
        showhelp(argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "fsck") == 0) {
        if (argc != 3) {
            fprintf(stderr, "usage: %s fsck <image>\n", argv[0]);
            return 1;
        }
        fsck(argv[2]);
        return 0;
    }

    if (strcmp(argv[1], "mkfs") == 0) {
        if (argc < 4) {
            fprintf(stderr, "usage: %s mkfs <image> <file1> [file2 ...]\n", argv[0]);
            return 1;
        }
        const char* output_path = argv[2];
        int file_count = argc - 3;

        if (file_count > MAX_FILES) {
            fprintf(stderr, "Max %d files\n", MAX_FILES);
            return 1;
        }

        FILE* out = fopen(output_path, "wb");
        if (!out) {
            perror("fopen");
            return 1;
        }

        Superblock sb = {
            .magic = "VFS1",
            .file_count = (uint32_t)file_count
        };
        fwrite(&sb, sizeof(Superblock), 1, out);

        FileEntry entries[MAX_FILES] = {0};

        write_padding(out, sizeof(FileEntry) * MAX_FILES);

        uint32_t offset = sizeof(Superblock) + sizeof(FileEntry) * MAX_FILES;

        for (int i = 0; i < file_count; i++) {
            const char* filename = argv[i + 3];
            FILE *in = fopen(filename, "rb");
            if (!in) {
                perror("fopen input");
                fclose(out);
                return 1;
            }

            fseek(in, 0, SEEK_END);
            long size = ftell(in);
            fseek(in, 0, SEEK_SET);

            strncpy(entries[i].name, filename, sizeof(entries[i].name) - 1);
            entries[i].size = (uint32_t)size;
            entries[i].offset = offset;

            char *buffer = malloc((size_t)size);
            if (!buffer) {
                fprintf(stderr, "malloc failed\n");
                fclose(in);
                fclose(out);
                return 1;
            }

            fread(buffer, 1, (size_t)size, in);
            fwrite(buffer, 1, (size_t)size, out);
            free(buffer);
            fclose(in);

            offset += (uint32_t)size;
        }

        fseek(out, sizeof(Superblock), SEEK_SET);
        fwrite(entries, sizeof(FileEntry), MAX_FILES, out);

        fclose(out);
        printf("Image created: %s (%d files)\n", output_path, file_count);
        return 0;
    }

    fprintf(stderr, "Unknown option '%s'. Use -h for help.\n", argv[1]);
    return 1;
}
