#include "directory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void entry(uint8_t *p, const char *name, uint8_t attributes)
{
    memset(p, 0, 32);
    memcpy(p, name, 11);
    p[11] = attributes;
}

int main(int argc, char **argv)
{
    uint8_t entries[7 * 32] = {0}, *root, *directory;
    FILE *image;
    unsigned bytes, reserved, fats, fat_size, root_count, sectors_per_cluster, cluster;
    int index;
    long root_offset;

    entry(entries, "DELETED TXT", 0); entries[0] = 0xe5;
    entry(entries + 32, "LONGNAMETXT", 0x0f);
    entry(entries + 64, "ICESTAR    ", 0x18);
    entry(entries + 96, "ICESTAR    ", 0x10);
    entry(entries + 128, "DAT     BIN", 0x20);
    entry(entries + 160, "12345678TXT", 0x20);
    assert(init_search_directory(entries, 7, "ICESTAR", 1) == 3);
    assert(init_search_directory(entries, 7, "DAT.BIN", 0) == 4);
    assert(init_search_directory(entries, 7, "12345678.TXT", 0) == 5);
    assert(init_search_directory(entries, 7, "dat.bin", 0) == -1);
    assert(init_search_directory(entries, 4, "DAT.BIN", 0) == -1);
    entries[4 * 32] = 0;
    assert(init_search_directory(entries, 7, "12345678.TXT", 0) == -1);

    assert(argc == 2 && (image = fopen(argv[1], "rb")) != NULL);
    assert(fseek(image, 17L * 512, SEEK_SET) == 0);
    assert(fread(entries, 1, 32, image) == 32);
    bytes = entries[11] | entries[12] << 8;
    reserved = entries[14] | entries[15] << 8;
    fats = entries[16];
    sectors_per_cluster = entries[13];
    root_count = entries[17] | entries[18] << 8;
    fat_size = entries[22] | entries[23] << 8;
    root_offset = 17L * 512 + (long)(reserved + fats * fat_size) * bytes;
    root = malloc(root_count * 32);
    assert(root && fseek(image, root_offset, SEEK_SET) == 0);
    assert(fread(root, 32, root_count, image) == root_count);
    index = init_search_directory(root, root_count, "ICESTAR", 1);
    assert(index >= 0);
    cluster = root[index * 32 + 26] | root[index * 32 + 27] << 8;
    assert(cluster >= 2 && sectors_per_cluster != 0);
    directory = malloc(bytes * sectors_per_cluster);
    assert(directory);
    assert(fseek(image, root_offset + ((root_count * 32 + bytes - 1) / bytes +
                 (cluster - 2) * sectors_per_cluster) * bytes, SEEK_SET) == 0);
    assert(fread(directory, bytes, sectors_per_cluster, image) == sectors_per_cluster);
    assert(init_search_directory(directory, bytes * sectors_per_cluster / 32, "DAT.BIN", 0) >= 0);
    assert(init_search_directory(directory, bytes * sectors_per_cluster / 32, "ICESTAR.X", 0) >= 0);
    free(directory);
    free(root);
    fclose(image);
    return 0;
}
