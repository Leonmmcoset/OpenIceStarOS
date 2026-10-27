#include "fat16.h"

static uint16_t le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)le16(p) | ((uint32_t)le16(p + 2) << 16);
}

int fat16_mount(struct fat16_volume *v, fat16_read_sector read,
                void *context, uint32_t start)
{
    uint8_t sector[512];
    uint32_t total, root_sectors, overhead, clusters;
    uint16_t reserved, fat_size, entries;
    uint8_t fats, spc;

    if (!v || !read || read(context, start, sector)) return FAT16_IO;
    reserved = le16(sector + 14);
    fats = sector[16];
    entries = le16(sector + 17);
    fat_size = le16(sector + 22);
    spc = sector[13];
    total = le16(sector + 19);
    if (!total) total = le32(sector + 32);
    if (le16(sector + 11) != 512 || sector[510] != 0x55 || sector[511] != 0xaa ||
        !reserved || !fats || !entries || !fat_size || !spc || (spc & (spc - 1)))
        return FAT16_FORMAT;
    root_sectors = ((uint32_t)entries * 32 + 511) / 512;
    overhead = reserved + (uint32_t)fats * fat_size + root_sectors;
    if (total <= overhead || start > UINT32_MAX - total) return FAT16_FORMAT;
    clusters = (total - overhead) / spc;
    if (clusters < 4085 || clusters >= 65525 ||
        (uint32_t)fat_size * 256 < clusters + 2) return FAT16_FORMAT;
    v->read = read;
    v->context = context;
    v->fat_lba = start + reserved;
    v->root_lba = v->fat_lba + (uint32_t)fats * fat_size;
    v->data_lba = start + overhead;
    v->cluster_count = clusters;
    v->root_entries = entries;
    v->fat_sectors = fat_size;
    v->sectors_per_cluster = spc;
    return FAT16_OK;
}

int fat16_read_root_file(const struct fat16_volume *v, const uint8_t name[11],
                         uint8_t *output, uint32_t capacity, uint32_t *size)
{
    uint8_t sector[512];
    uint32_t i, j, remaining, cluster, traversed = 0, written = 0;
    int found = 0;

    if (!v || !name || !size || !v->read) return FAT16_FORMAT;
    for (i = 0; i < v->root_entries; ++i) {
        const uint8_t *entry;
        if (!(i % 16) && v->read(v->context, v->root_lba + i / 16, sector))
            return FAT16_IO;
        entry = sector + (i % 16) * 32;
        if (!entry[0]) break;
        if (entry[0] == 0xe5 || (entry[11] & 0x18)) continue;
        for (j = 0; j < 11 && entry[j] == name[j]; ++j) { }
        if (j == 11) {
            cluster = le16(entry + 26);
            remaining = le32(entry + 28);
            found = 1;
            break;
        }
    }
    if (!found) return FAT16_NOT_FOUND;
    *size = remaining;
    if (remaining > capacity || (remaining && !output)) return FAT16_NO_SPACE;
    while (remaining) {
        uint32_t lba;
        if (cluster < 2 || cluster >= v->cluster_count + 2 ||
            cluster >= 0xfff0 || ++traversed > v->cluster_count)
            return FAT16_CHAIN;
        lba = v->data_lba + (cluster - 2) * v->sectors_per_cluster;
        for (i = 0; i < v->sectors_per_cluster && remaining; ++i) {
            uint32_t count = remaining < 512 ? remaining : 512;
            if (v->read(v->context, lba + i, sector)) return FAT16_IO;
            for (j = 0; j < count; ++j) output[written + j] = sector[j];
            written += count;
            remaining -= count;
        }
        if (remaining) {
            uint32_t offset = cluster * 2;
            if (v->read(v->context, v->fat_lba + offset / 512, sector)) return FAT16_IO;
            cluster = le16(sector + offset % 512);
        }
    }
    return FAT16_OK;
}
