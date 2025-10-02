
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "vfs.h"

int compare(const void* a, const void* b) {
    const FileEntry *fa = (const FileEntry *)a;
    const FileEntry *fb = (const FileEntry *)b;
    if (fa->offset < fb->offset) return -1;
    if (fa->offset > fb->offset) return 1;
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <image>\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
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
    printf("Check %s:\n", argv[1]);

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
