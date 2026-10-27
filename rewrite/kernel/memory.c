/* Heap services recovered from 0x815464..0x815a90. Keep original unchecked
 * 32-bit arithmetic, header stores, first-fit decisions, and merge behavior.
 * The fixed heap starts at 32 MiB; each header is next/previous/used/owner. */
#include "memory.h"

#define WORD(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define FIELD(node, offset) WORD((node) + (offset))

uint32_t kernel_read_flags(void);
void kernel_disable_interrupts(void);
void kernel_write_flags(uint32_t flags);

static __attribute__((always_inline)) inline uint32_t
allocate(uint32_t size, uint32_t owner, int current_owner)
{
    uint32_t flags = kernel_read_flags(), node = 0x2000000, span, following;
    kernel_disable_interrupts();
    size = (size + 0x40fu) & 0xfffffc00u;
    while (FIELD(node, 0) != 0) {
        span = FIELD(node, 0) - node;
        if (FIELD(node, 8) == 0 && span >= size) {
            FIELD(node, 8) = 1;
            FIELD(node, 12) = current_owner ? WORD(0x1b0d8) : owner;
            if (span - size > 0x40fu) {
                following = node + size;
                FIELD(following, 0) = FIELD(node, 0);
                FIELD(following, 4) = node;
                FIELD(following, 8) = 0;
                FIELD(following, 12) = 0;
                FIELD(node, 0) = following;
            }
            break;
        }
        node = FIELD(node, 0);
    }
    if (FIELD(node, 0) == 0 && FIELD(node, 8) == 0) {
        FIELD(node, 0) = node + size;
        if (FIELD(node, 0) >= WORD(0x1b050)) {
            FIELD(node, 0) = 0;
            kernel_write_flags(flags);
            return 0;
        }
        FIELD(node, 8) = 1;
        FIELD(node, 12) = current_owner ? WORD(0x1b0d8) : owner;
        following = FIELD(node, 0);
        FIELD(following, 0) = 0;
        FIELD(following, 4) = node;
        FIELD(following, 8) = 0;
        FIELD(following, 12) = 0;
    }
    kernel_write_flags(flags);
    return node + 16u;
}

uint32_t kernel_allocate(uint32_t size)
{
    return allocate(size, 0, 1);
}

uint32_t kernel_allocate_for_task(uint32_t size, uint32_t owner)
{
    return allocate(size, owner, 0);
}

static __attribute__((always_inline)) inline uint32_t
aligned_address(uint32_t node, uint32_t alignment)
{
    /* The original mixes unsigned size division with signed address division.
       Zero divisors and signed quotient overflow retain x86 divide faults. */
    int32_t quotient = (int32_t)(node + alignment + 15u) / (int32_t)alignment;
    return (uint32_t)quotient * alignment;
}

uint32_t kernel_allocate_aligned(uint32_t size, uint32_t owner, uint32_t alignment)
{
    uint32_t flags = kernel_read_flags(), node = 0x2000000;
    uint32_t span, address, previous, following;
    kernel_disable_interrupts();
    size = ((size + alignment - 1u) / alignment) * alignment;
    while (FIELD(node, 0) != 0) {
        address = aligned_address(node, alignment);
        if (FIELD(node, 0) > address) span = FIELD(node, 0) - address;
        else span = 0;
        if (FIELD(node, 8) == 0 && span >= size) {
            previous = FIELD(node, 4);
            following = FIELD(node, 0);
            FIELD(previous, 0) = address - 16u;
            node = address - 16u;
            FIELD(node, 0) = following;
            FIELD(node, 4) = previous;
            FIELD(node, 8) = 1;
            FIELD(node, 12) = owner;
            if (span - size > 0x40fu) {
                following = address + size;
                FIELD(following, 0) = FIELD(node, 0);
                FIELD(following, 4) = node;
                FIELD(following, 8) = 0;
                FIELD(following, 12) = 0;
                FIELD(node, 0) = following;
            }
            break;
        }
        node = FIELD(node, 0);
    }
    if (FIELD(node, 0) == 0 && FIELD(node, 8) == 0) {
        address = aligned_address(node, alignment);
        FIELD(node, 0) = address + size;
        if (FIELD(node, 0) >= WORD(0x1b050)) {
            FIELD(node, 0) = 0;
            kernel_write_flags(flags);
            return 0;
        }
        previous = FIELD(node, 4);
        following = FIELD(node, 0);
        FIELD(previous, 0) = address - 16u;
        node = address - 16u;
        FIELD(node, 0) = following;
        FIELD(node, 4) = previous;
        FIELD(node, 8) = 1;
        FIELD(node, 12) = owner;
        following = FIELD(node, 0);
        FIELD(following, 0) = 0;
        FIELD(following, 4) = node;
        FIELD(following, 8) = 0;
        FIELD(following, 12) = 0;
    }
    kernel_write_flags(flags);
    return node + 16u;
}

void kernel_free_allocation(uint32_t address)
{
    uint32_t flags = kernel_read_flags(), node, adjacent;
    kernel_disable_interrupts();
    if (address != 0) {
        node = address - 16u;
        FIELD(node, 8) = 0;
        FIELD(node, 12) = 0;
        if (FIELD(node, 4) != 0) {
            adjacent = FIELD(node, 4);
            if (FIELD(adjacent, 8) == 0) {
                FIELD(adjacent, 0) = FIELD(node, 0);
                node = adjacent;
            }
        }
        if (FIELD(node, 0) != 0) {
            adjacent = FIELD(node, 0);
            if (FIELD(adjacent, 8) == 0) FIELD(node, 0) = FIELD(adjacent, 0);
        }
    }
    kernel_write_flags(flags);
}

uint32_t kernel_allocation_size(uint32_t address)
{
    uint32_t node;
    if (address != 0) {
        node = address - 16u;
        if (FIELD(node, 0) != 0) return FIELD(node, 0) - address;
    }
    return 0;
}

uint32_t kernel_task_allocation_total(uint32_t owner)
{
    uint32_t total = 0, node = 0x2000000;
    while (FIELD(node, 0) != 0) {
        if (FIELD(node, 8) == 1 && FIELD(node, 12) == owner)
            total = total + FIELD(node, 0) - node;
        node = FIELD(node, 0);
    }
    return total;
}

#undef FIELD
#undef WORD
