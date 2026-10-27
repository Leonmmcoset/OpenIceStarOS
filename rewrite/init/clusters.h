#ifndef ICESTAR_INIT_CLUSTERS_H
#define ICESTAR_INIT_CLUSTERS_H

#include <stdint.h>

typedef void (*init_sector_reader)(void *context, uint32_t lba, uint8_t *destination);

struct init_disk {
    const uint8_t *bpb;
    const uint8_t *fat;
    uint32_t partition_lba;
    init_sector_reader read_sector;
    void *context;
};

int init_read_chain(const struct init_disk *disk, const uint8_t *entry,
                    uint8_t *destination);

#endif
