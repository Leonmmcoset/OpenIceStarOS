#include "clusters.h"
#include "directory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t word(const uint8_t *p)
{
    return (uint16_t)(p[0] | (uint16_t)p[1] << 8);
}

static uint32_t dword(const uint8_t *p)
{
    return word(p) | (uint32_t)word(p + 2) << 16;
}

static void read_sector(void *context, uint32_t lba, uint8_t *destination)
{
    FILE *image = context;
    assert(fseek(image, (long)lba * 512, SEEK_SET) == 0);
    assert(fread(destination, 1, 512, image) == 512);
}

int main(int argc, char **argv)
{
    struct init_disk disk;
    uint8_t bpb[512], *fat, *root, *buffer;
    unsigned fat_bytes, root_bytes, i;
    int index, count;
    FILE *image, *output;
    const char *names[] = {"DAT.BIN", "ICESTAR.X"};

    assert(argc == 4 && (image = fopen(argv[1], "rb")) != NULL);
    read_sector(image, 17, bpb);
    assert(word(bpb + 11) == 512);
    fat_bytes = word(bpb + 22) * 512;
    root_bytes = word(bpb + 17) * 32;
    fat = malloc(fat_bytes);
    root = malloc(root_bytes);
    buffer = malloc(1024 * 1024);
    assert(fat && root && buffer);
    for (i = 0; i < fat_bytes / 512; ++i)
        read_sector(image, 17 + word(bpb + 14) + i, fat + i * 512);
    for (i = 0; i < root_bytes / 512; ++i)
        read_sector(image, 17 + word(bpb + 14) + bpb[16] * word(bpb + 22) + i,
                    root + i * 512);
    disk.bpb = bpb;
    disk.fat = fat;
    disk.partition_lba = 17;
    disk.read_sector = read_sector;
    disk.context = image;
    index = init_search_directory(root, word(bpb + 17), "ICESTAR", 1);
    assert(index >= 0);
    for (i = 0; i < 2; ++i) {
        uint32_t size;
        count = init_read_chain(&disk, root + index * 32, buffer);
        assert(count > 0);
        count = init_search_directory(buffer, 512 * bpb[13] / 32, names[i], 0);
        assert(count >= 0);
        size = dword(buffer + count * 32 + 28);
        count = init_read_chain(&disk, buffer + count * 32, buffer);
        assert(count > 0 && size <= (uint32_t)count * 512 * bpb[13]);
        output = fopen(argv[i + 2], "wb");
        assert(output && fwrite(buffer, 1, size, output) == size);
        assert(fclose(output) == 0);
    }
    free(buffer);
    free(root);
    free(fat);
    fclose(image);
    return 0;
}
