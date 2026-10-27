#include <stdio.h>
#include <stdlib.h>
#include "fat16.h"

static int read_sector(void *context, uint32_t lba, uint8_t *buffer)
{
    FILE *file = context;
    return fseek(file, (long)lba * 512, SEEK_SET) || fread(buffer, 1, 512, file) != 512;
}

int main(int argc, char **argv)
{
    static const uint8_t name[11] = "INIT    X  ";
    struct fat16_volume volume;
    uint8_t *data;
    uint32_t size = 0;
    FILE *image, *output;
    int result;

    if (argc != 3 || !(image = fopen(argv[1], "rb"))) return 1;
    result = fat16_mount(&volume, read_sector, image, 17);
    if (result != FAT16_OK) { fprintf(stderr, "mount: %d\n", result); return 1; }
    result = fat16_read_root_file(&volume, name, NULL, 0, &size);
    if (result != FAT16_NO_SPACE || size != 26817) {
        fprintf(stderr, "file size: %u (result %d)\n", size, result); return 1;
    }
    data = malloc(size);
    if (!data) return 1;
    result = fat16_read_root_file(&volume, name, data, size, &size);
    if (result != FAT16_OK) { fprintf(stderr, "read: %d\n", result); return 1; }
    output = fopen(argv[2], "wb");
    if (!output || fwrite(data, 1, size, output) != size || fclose(output)) return 1;
    free(data);
    fclose(image);
    return 0;
}
