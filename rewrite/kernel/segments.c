/* Segment descriptor emission, recovered from 0x8135c6..0x81364c.
 * Preserve the two WORD stores followed by four BYTE stores. */
#include "helpers.h"

void kernel_set_segment_descriptor(uint32_t address, uint32_t limit,
                                   uint32_t base, uint32_t access)
{
    if (limit > 0xfffffu) {
        access |= 0x8000;
        limit >>= 12;
    }
    *(volatile uint16_t *)(uintptr_t)address = (uint16_t)limit;
    *(volatile uint16_t *)(uintptr_t)(address + 2u) = (uint16_t)base;
    *(volatile uint8_t *)(uintptr_t)(address + 4u) = (uint8_t)(base >> 16);
    *(volatile uint8_t *)(uintptr_t)(address + 5u) = (uint8_t)access;
    *(volatile uint8_t *)(uintptr_t)(address + 6u) =
        (uint8_t)(((limit >> 16) & 15u) | ((access >> 8) & 0xf0u));
    *(volatile uint8_t *)(uintptr_t)(address + 7u) = (uint8_t)(base >> 24);
}
