#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include "vfs.h"

void write_padding(FILE* f, size_t count) {
    char zero = 0;
    for (size_t i = 0; i < count; i++) {
        fwrite(&zero, 1, 1, f);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <image> <file1> [file2 ...]\n", argv[0]);
        return 1;
    }

    const char* output_path = argv[1];
    int file_count = argc - 2;

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
        const char* filename = argv[i + 2];
        FILE *in = fopen(filename, "rb");
        if (!in) {
            perror("fopen input");
            return 1;
        }

        fseek(in, 0, SEEK_END);
        long size = ftell(in);
        fseek(in, 0, SEEK_SET);

        strncpy(entries[i].name, filename, sizeof(entries[i].name) - 1);
        entries[i].size = (uint32_t)size;
        entries[i].offset = offset;

        char *buffer = malloc((size_t)size);
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
