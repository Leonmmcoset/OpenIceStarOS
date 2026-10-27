#ifndef ICESTAR_FAT16_H
#define ICESTAR_FAT16_H

#include <stdint.h>

typedef int (*fat16_read_sector)(void *context, uint32_t lba, uint8_t *buffer);

struct fat16_volume {
    fat16_read_sector read;
    void *context;
    uint32_t fat_lba, root_lba, data_lba, cluster_count;
    uint16_t root_entries, fat_sectors;
    uint8_t sectors_per_cluster;
};

enum fat16_result {
    FAT16_OK = 0, FAT16_IO = -1, FAT16_FORMAT = -2,
    FAT16_NOT_FOUND = -3, FAT16_NO_SPACE = -4, FAT16_CHAIN = -5
};

int fat16_mount(struct fat16_volume *volume, fat16_read_sector read,
                void *context, uint32_t partition_lba);
int fat16_read_root_file(const struct fat16_volume *volume, const uint8_t name[11],
                         uint8_t *output, uint32_t capacity, uint32_t *size);

#endif
