/* Directory-only absolute lookup, 0x825f48..0x8263c0. Preserve the
 * 32x13 parser, signed short-name comparison, one-cluster scan bounds,
 * and free-before-copy/read behavior of the original directory walk. */
#include "fat_path.h"
#include "fat_files.h"
#include "memory.h"
#include "memory_accel.h"
#include "string_extra.h"
#define B(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define W(a) (*(volatile uint16_t *)(uintptr_t)(uint32_t)(a))
#define BPB 0x520000u

static __attribute__((always_inline)) inline int matches(uint32_t record, const int8_t *name)
{
    for (uint32_t i = 0; i < 11; ++i)
        if ((int32_t)name[i] != (int32_t)B(record + i)) return 0;
    return 1;
}

int32_t kernel_fat_find_directory_path_825f48(uint32_t name, uint32_t output)
{
    struct {
        int8_t short_name[16];
        int8_t parts[32][13];
    } local;
    int8_t (*parts)[13] = local.parts;
    int8_t *short_name = local.short_name;
    uint32_t count = 0, position = 0, ended = 0;
    if (!B(name)) return -1;
    for (uint32_t part = 0; part < 32; ++part) {
        for (uint32_t character = 0; character < 13; ++character) {
            if (!B(name + position)) {
                parts[part][character] = 0;
                ended = 1;
                if (character > 0) {
                    ++position;
                    ++count;
                    (void)kernel_string_upper_82a531((uint32_t)(uintptr_t)parts[part]);
                }
                break;
            }
            if (B(name + position) == '\\') {
                parts[part][character] = 0;
                ++position;
                if (!character) return -1;
                ++count;
                (void)kernel_string_upper_82a531((uint32_t)(uintptr_t)parts[part]);
                break;
            }
            parts[part][character] = (int8_t)B(name + position++);
            if (character == 12) return -1;
        }
        if (ended) break;
    }
    if (!count) return -1;
    int32_t found = 0;
    if (parts[0][0] != 'C' || parts[0][1] != ':') return found;
    int32_t cluster_bytes = B(BPB + 13u) * W(BPB + 11u);
    uint32_t root = W(BPB + 14u) + B(BPB + 16u) * W(BPB + 22u);
    uint32_t directory = BPB + root * W(BPB + 11u), entries = W(BPB + 17u);
    for (uint32_t part = 1; part < count; ++part) {
        found = 0;
        (void)kernel_fat_name83_827bba((uint32_t)(uintptr_t)parts[part],
                                     (uint32_t)(uintptr_t)short_name);
        for (uint32_t index = 0; index < entries; ++index) {
            uint32_t record = directory + (index << 5);
            if (!B(record)) break;
            if (B(record) == 0xe5 || !(B(record + 11u) & 16u) || !matches(record, short_name)) continue;
            found = 1;
            entries = (uint32_t)(cluster_bytes / 32);
            if (part > 1) kernel_free_allocation(directory);
            if (part == count - 1u)
                (void)kernel_rep_copy_bytes(output, record, 32);
            else
                directory = kernel_fat_read_alloc_827454(record);
            break;
        }
        if (!found) {
            if (part > 1) kernel_free_allocation(directory);
            return 0;
        }
    }
    return found;
}
