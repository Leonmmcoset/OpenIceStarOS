/* Object lookup/state/registry helpers at 0x8104a0..0x81067a. Arithmetic
 * wraps in 32 bits. Keep the captured table/capacity, live count and exact
 * original accesses, including removal's extra read at the old count. */
#include "object_registry.h"
#include "helpers.h"

#define DWORD(address) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(address))

uint32_t kernel_object_find_name(uint32_t name)
{
    uint32_t capacity = DWORD(0x1b098), index;
    for (index = 1; (int32_t)index < (int32_t)capacity; ++index) {
        if ((int32_t)DWORD(0x6a0000u + index * 4u) > 0) {
            uint32_t object = DWORD(0x6a0000u + index * 4u);
            if (kernel_strings_equal(object + 16u, name) == 1u)
                return index;
        }
    }
    return 0;
}

uint32_t kernel_object_set_state(uint32_t object, uint32_t state)
{
    uint32_t record = DWORD(0x6a0000u + object * 4u);
    DWORD(record + 0x127u) = state;
    return state;
}

uint32_t kernel_object_registry_add(uint32_t object)
{
    uint32_t table = DWORD(0x1b0f8), capacity = DWORD(0x1b0f0), index;
    for (index = 0; (int32_t)index < (int32_t)DWORD(0x1b0f4); ++index)
        if (DWORD(table + index * 4u) == object) return index;
    if ((int32_t)DWORD(0x1b0f4) >= (int32_t)capacity) return 0xffffffffu;
    index = DWORD(0x1b0f4);
    DWORD(table + index * 4u) = object;
    DWORD(0x1b0f4) = DWORD(0x1b0f4) + 1u;
    return DWORD(0x1b0f4) - 1u;
}

uint32_t kernel_object_registry_remove(uint32_t object)
{
    uint32_t table = DWORD(0x1b0f8), index, found = 0xffffffffu;
    (void)DWORD(0x1b0f0); /* The original reads capacity even though unused. */
    for (index = 0; (int32_t)index < (int32_t)DWORD(0x1b0f4); ++index) {
        if (DWORD(table + index * 4u) == object) {
            found = index;
            break;
        }
    }
    if ((int32_t)found < 0) return found;
    for (index = found; (int32_t)index < (int32_t)DWORD(0x1b0f4); ++index)
        DWORD(table + index * 4u) = DWORD(table + index * 4u + 4u);
    DWORD(0x1b0f4) = DWORD(0x1b0f4) - 1u;
    return found;
}
