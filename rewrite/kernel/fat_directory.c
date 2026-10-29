/* Original directory services at 0x8258a6..0x825f48 and
 * 0x826940..0x826ade. Retain bytewise alias effects, case-sensitive
 * lookup, root-only volume-label status, and extension pointer returns. */
#include "fat_directory.h"
#include "helpers.h"
#define B(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define W(a) (*(volatile uint16_t *)(uintptr_t)(uint32_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define BPB 0x520000u

static __attribute__((always_inline)) inline uint32_t root_record(uint32_t index)
{
    uint32_t reserved = W(BPB + 14u), fats = B(BPB + 16u);
    uint32_t sectors = reserved + fats * W(BPB + 22u);
    return BPB + sectors * W(BPB + 11u) + (index << 5);
}

static __attribute__((always_inline)) inline int32_t extract_name(
    uint32_t record, uint32_t output, uint32_t root)
{
    if (!B(record)) return 0;
    if (B(record) == 0xe5u) return -1;
    if (B(record + 11u) == 0x0fu) return -2;
    uint32_t position = 0;
    while ((int32_t)position <= 7) {
        B(output + position) = B(record + position);
        if (B(output + position) == ' ' && B(record + 8u) != ' ') {
            B(output + position++) = '.';
            break;
        }
        if (B(output + position) == ' ' && B(record + 8u) == ' ') {
            B(output + position) = 0;
            return root && (B(record + 11u) & 8u) ? -3 : 1;
        }
        ++position;
    }
    if (position == 8 && B(output + position - 1u) != '.' && B(record + 8u) != ' ')
        B(output + position++) = '.';
    for (uint32_t extension = 0; (int32_t)extension <= 2; ++extension) {
        B(output + position) = B(record + 8u + extension);
        if (B(output + position) == ' ') {
            B(output + position) = 0;
            return root && (B(record + 11u) & 8u) ? -3 : 1;
        }
        ++position;
    }
    B(output + position) = 0;
    return root && (B(record + 11u) & 8u) ? -3 : 1;
}

__attribute__((noinline)) int32_t kernel_fat_root_name_8258a6(uint32_t index, uint32_t output)
{
    return extract_name(root_record(index), output, 1);
}

__attribute__((noinline)) int32_t kernel_fat_directory_name_825a7e(
    uint32_t directory, uint32_t index, uint32_t output)
{
    return extract_name(directory + (index << 5), output, 0);
}

uint32_t kernel_fat_root_extension_825bcc(uint32_t index, uint32_t output)
{
    uint32_t record = root_record(index);
    B(output) = B(record + 8u);
    B(output + 1u) = B(record + 9u);
    B(output + 2u) = B(record + 10u);
    B(output + 3u) = 0;
    return output;
}

uint32_t kernel_fat_directory_extension_825c46(uint32_t directory, uint32_t index, uint32_t output)
{
    uint32_t record = directory + (index << 5);
    B(output) = B(record + 8u);
    B(output + 1u) = B(record + 9u);
    B(output + 2u) = B(record + 10u);
    B(output + 3u) = 0;
    return output;
}

int32_t kernel_x_find_relative_825c8e(uint32_t name)
{
    uint8_t filename[32];
    for (uint32_t index = 0; (int32_t)index < (int32_t)W(BPB + 17u); ++index) {
        int32_t status = kernel_fat_root_name_8258a6(index, (uint32_t)(uintptr_t)filename);
        if (status == 1) {
            if (kernel_strings_equal((uint32_t)(uintptr_t)filename, name) == 1) {
                uint8_t attributes = (uint8_t)kernel_fat_root_attributes_8269a6(index);
                if (!(attributes & 24u)) return (int32_t)index;
            }
        } else if (!status) break;
    }
    return -1;
}

int32_t kernel_fat_directory_find_825d34(uint32_t directory, uint32_t name)
{
    int32_t bytes = W(BPB + 11u) * B(BPB + 13u);
    int32_t entries = bytes / 32;
    uint8_t filename[32];
    for (uint32_t index = 0; (int32_t)index < entries; ++index) {
        int32_t status = kernel_fat_directory_name_825a7e(directory, index,
                                                       (uint32_t)(uintptr_t)filename);
        if (status == 1) {
            if (kernel_strings_equal((uint32_t)(uintptr_t)filename, name) == 1) {
                uint8_t attributes = (uint8_t)kernel_fat_directory_attributes_8269f2(directory, index);
                if (!(attributes & 24u)) return (int32_t)index;
            }
        } else if (!status) break;
    }
    return -1;
}

uint32_t kernel_fat_root_size_826940(uint32_t index)
{
    return D(root_record(index) + 28u);
}

uint32_t kernel_fat_directory_size_82698c(uint32_t directory, uint32_t index)
{
    return D(directory + (index << 5) + 28u);
}

__attribute__((noinline)) uint32_t kernel_fat_root_attributes_8269a6(uint32_t index)
{
    return B(root_record(index) + 11u);
}

__attribute__((noinline)) uint32_t kernel_fat_directory_attributes_8269f2(uint32_t directory, uint32_t index)
{
    return B(directory + (index << 5) + 11u);
}

uint32_t kernel_fat_root_date_826a0e(uint32_t index)
{
    return W(root_record(index) + 24u);
}

uint32_t kernel_fat_directory_date_826a5a(uint32_t directory, uint32_t index)
{
    return W(directory + (index << 5) + 24u);
}

uint32_t kernel_fat_root_time_826a76(uint32_t index)
{
    return W(root_record(index) + 22u);
}

uint32_t kernel_fat_directory_time_826ac2(uint32_t directory, uint32_t index)
{
    return W(directory + (index << 5) + 22u);
}

int32_t kernel_fat_root_find_directory_825df4(uint32_t name)
{
    uint8_t filename[32];
    for (uint32_t index = 0; (int32_t)index < (int32_t)W(BPB + 17u); ++index) {
        int32_t status = kernel_fat_root_name_8258a6(index, (uint32_t)(uintptr_t)filename);
        if (status == 1) {
            if (kernel_strings_equal((uint32_t)(uintptr_t)filename, name) == 1) {
                uint8_t attributes = (uint8_t)kernel_fat_root_attributes_8269a6(index);
                if (attributes & 16u) return (int32_t)index;
            }
        } else if (!status) break;
    }
    return -1;
}

int32_t kernel_fat_find_directory_825e86(uint32_t directory, uint32_t name)
{
    int32_t bytes = W(BPB + 11u) * B(BPB + 13u);
    int32_t entries = bytes / 32;
    uint8_t filename[32];
    for (uint32_t index = 0; (int32_t)index < entries; ++index) {
        int32_t status = kernel_fat_directory_name_825a7e(directory, index,
                                                       (uint32_t)(uintptr_t)filename);
        if (status == 1) {
            if (kernel_strings_equal((uint32_t)(uintptr_t)filename, name) == 1) {
                uint8_t attributes = (uint8_t)kernel_fat_directory_attributes_8269f2(directory, index);
                if (attributes & 16u) return (int32_t)index;
            }
        } else if (!status) break;
    }
    return -1;
}
