/* FAT16 path splitting and root-file writing recovered from 0x828018 and
 * 0x82867e. Retain legacy unchecked buffers, wrap arithmetic, partial-cluster
 * over-writes and free-cluster failure behavior for binary compatibility. */
#include "fat_write.h"
#include "fat_directory.h"
#include "fat_path.h"
#include "fat_slots.h"
#include "fat_mutation.h"
#include "fat_metadata.h"
#include "fat_io.h"
#include "helpers.h"
#include "string_extra.h"

#define B(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define W(a) (*(volatile uint16_t *)(uintptr_t)(uint32_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define BPB 0x520000u

uint32_t kernel_fat_split_parent_828018(uint32_t name, uint32_t parent,
                                      uint32_t filename)
{
    uint32_t length = kernel_string_length(name);
    int32_t last = (int32_t)(length - 1u), separator = 0;
    while (last > 0) {
        if (B(name + (uint32_t)last) == '\\') {
            separator = last;
            break;
        }
        last = (int32_t)((uint32_t)last - 1u);
    }
    if (separator <= 0) return 0;
    uint32_t i;
    for (i = 0; i < (uint32_t)separator; ++i) B(parent + i) = B(name + i);
    B(parent + i) = 0;
    int32_t count = (int32_t)(length - (uint32_t)separator - 1u);
    for (i = 0; (int32_t)i < count; ++i)
        B(filename + i) = B(name + (uint32_t)separator + i + 1u);
    B(filename + i) = 0;
    return 1;
}

static __attribute__((always_inline)) inline uint32_t root_record(uint32_t index)
{
    uint32_t reserved = W(BPB + 14u);
    uint32_t fats = B(BPB + 16u);
    uint32_t sectors = W(BPB + 22u);
    uint32_t bps = W(BPB + 11u);
    return BPB + (reserved + fats * sectors) * bps + (index << 5);
}

static __attribute__((always_inline)) inline uint32_t data_lba(void)
{
    uint32_t rounded = W(BPB + 17u) * 32u + W(BPB + 11u) - 1u;
    int32_t root_sectors = (int32_t)rounded / (int32_t)W(BPB + 11u);
    uint32_t sectors = W(BPB + 22u);
    uint32_t fats = B(BPB + 16u);
    uint32_t reserved = W(BPB + 14u);
    return (uint32_t)root_sectors + fats * sectors + reserved + D(0x7dc6u);
}

static __attribute__((always_inline)) inline void stamp_record(uint32_t record,
                                                               uint32_t short_name)
{
    for (uint32_t i = 0; i < 8; ++i) B(record + i) = B(short_name + i);
    for (uint32_t i = 0; i < 3; ++i) B(record + 8u + i) = B(short_name + 8u + i);
    B(record + 11u) = 0x20u;
    B(record + 12u) = 0x18u;
    B(record + 13u) = 0x12u;
    W(record + 14u) = 0x8edcu;
    W(record + 16u) = 0x5836u;
    W(record + 18u) = 0x5836u;
    W(record + 20u) = 0;
    W(record + 22u) = 0x8eddu;
    W(record + 24u) = 0x5836u;
}

static __attribute__((always_inline)) inline uint16_t record_cluster(uint32_t record)
{
    uint32_t high = W(record + 20u);
    return (uint16_t)((high << 16) + W(record + 26u));
}

/* The second FAT is copied from FAT1 only after all data writes. Three
 * separate volatile reads preserve the original behavior for aliased tables. */
static __attribute__((always_inline)) inline void mirror_chain(uint32_t fat1,
                                                               uint32_t fat2,
                                                               uint16_t cluster)
{
    for (;;) {
        uint32_t first = fat1 + (uint32_t)cluster * 2u;
        uint32_t second = fat2 + (uint32_t)cluster * 2u;
        W(second) = W(first);
        cluster = W(first);
        if (W(first) == 0xffffu) break;
    }
}

uint32_t kernel_fat_write_root_82867e(uint32_t name, uint32_t initial_source,
                                    uint32_t bytes)
{
    /* Keep the advancing cursor separate from the caller's parameter slot,
     * as in the original function's cdecl stack behavior. */
    volatile uint32_t source = initial_source;
    uint32_t fat1 = D(0x1a83au), fat2 = D(0x1a83eu);
    uint32_t start = data_lba();
    uint8_t local[128];
    uint32_t text = (uint32_t)(uintptr_t)local;
    (void)kernel_copy_string(text, name);
    (void)kernel_string_upper_82a531(text);
    int32_t index = kernel_x_find_relative_825c8e(text);
    uint32_t sectors = (bytes + W(BPB + 11u) - 1u) / W(BPB + 11u);
    uint32_t clusters = (sectors + B(BPB + 13u) - 1u) / B(BPB + 13u);
    uint32_t remaining = clusters, record;
    if (index > 0) {
        record = root_record((uint32_t)index);
        (void)kernel_fat_release_root_chain_827d4a((uint32_t)index);
        D(record + 28u) = bytes;
    } else {
        index = kernel_fat_root_empty_82798c();
        if (index < 0) return 0;
        record = root_record((uint32_t)index);
        if (B(record) == 0xe5u)
            (void)kernel_fat_release_root_chain_827d4a((uint32_t)index);
        (void)kernel_fat_name83_827bba(name, text);
        stamp_record(record, text);
        W(record + 26u) = (uint16_t)kernel_fat_find_free_cluster_827ace();
        D(record + 28u) = bytes;
    }
    uint16_t cluster = record_cluster(record);
    uint32_t lba = ((uint32_t)cluster - 2u) * B(BPB + 13u) + start;
    if (remaining) {
        uint32_t count = B(BPB + 13u) > sectors ? sectors : B(BPB + 13u);
        kernel_fat_write_sectors_827c72(lba, source, count);
        --remaining;
    }
    while (remaining) {
        int32_t next = kernel_fat_find_free_after_827b44(cluster);
        /* The original tests != 0. In particular, -1 is not rejected. */
        if (next != 0) {
            W(fat1 + (uint32_t)cluster * 2u) = (uint16_t)next;
            cluster = (uint16_t)next;
            lba = ((uint32_t)cluster - 2u) * B(BPB + 13u) + start;
            source += (uint32_t)B(BPB + 13u) * W(BPB + 11u);
            kernel_fat_write_sectors_827c72(lba, source, B(BPB + 13u));
        }
        --remaining;
    }
    if (B(BPB + 16u)) W(fat1 + (uint32_t)cluster * 2u) = 0xffffu;
    if (B(BPB + 16u) > 1) mirror_chain(fat1, fat2, record_cluster(record));
    (void)kernel_fat_flush_metadata_827cc4();
    return sectors;
}
