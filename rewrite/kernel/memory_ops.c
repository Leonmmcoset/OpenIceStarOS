/* Recovered from 0x815a90..0x8173da and 0x817548..0x8175c0.
 * Volatile accesses retain byte/DWORD width and forward overlap behavior.
 * All address arithmetic wraps as unsigned 32-bit arithmetic on x86. */
#include "memory_ops.h"
#include "memory.h"
#define BYTE(address) (*(volatile uint8_t *)(uintptr_t)(address))
#define WORD(address) (*(volatile uint32_t *)(uintptr_t)(address))

uint32_t kernel_move_bytes(uint32_t destination, uint32_t source, uint32_t size)
{
    uint32_t result = destination;
    if (destination >= source && source + size >= destination) {
        while (size != 0) {
            --size;
            BYTE(destination + size) = BYTE(source + size);
        }
    } else {
        while (size != 0) {
            BYTE(destination) = BYTE(source);
            ++source;
            ++destination;
            --size;
        }
    }
    return result;
}

void kernel_fill_bytes(uint32_t destination, uint32_t value, uint32_t size)
{
    while (size != 0) {
        BYTE(destination) = (uint8_t)value;
        ++destination;
        --size;
    }
}

void kernel_copy_1200_bytes(uint32_t destination, uint32_t source)
{
    uint32_t offset;
    /* The original unrolls 300 DWORD transfers. It is not a byte memmove:
       partially overlapping DWORDs must propagate in this exact order. */
    for (offset = 0; offset < 1200; offset += 4)
        WORD(destination + offset) = WORD(source + offset);
}

void kernel_copy_dwords(uint32_t destination, uint32_t source, int32_t count)
{
    int32_t blocks = count / 300, remainder = count % 300, i;
    for (i = 0; i < blocks; ++i) {
        kernel_copy_1200_bytes(destination, source);
        source += 1200;
        destination += 1200;
    }
    for (i = 0; i < remainder; ++i)
        WORD(destination + (uint32_t)i * 4u) = WORD(source + (uint32_t)i * 4u);
}

uint32_t kernel_memory_differs(uint32_t first, uint32_t second, int32_t count)
{
    int32_t i;
    for (i = 0; i < count; ++i)
        if (BYTE(first + (uint32_t)i) != BYTE(second + (uint32_t)i)) return 1;
    return 0;
}

uint32_t kernel_malloc(uint32_t size)
{
    return kernel_allocate(size);
}

void kernel_free(uint32_t address)
{
    kernel_free_allocation(address);
}

#undef BYTE
#undef WORD
