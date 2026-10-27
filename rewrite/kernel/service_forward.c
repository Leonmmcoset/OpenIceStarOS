#include "service_forward.h"

#define SERVICE_VALUES_1 a0
#define SERVICE_VALUES_3 SERVICE_VALUES_1, a1, a2
#define SERVICE_VALUES_4 SERVICE_VALUES_3, a3
#define SERVICE_VALUES_5 SERVICE_VALUES_4, a4
#define SERVICE_VALUES_6 SERVICE_VALUES_5, a5
#define SERVICE_VALUES_7 SERVICE_VALUES_6, a6
#define SERVICE_VALUES_6S SERVICE_VALUES_5, (uint32_t)(int32_t)(int8_t)a5

/* Capture the shared table once, just like the original wrappers. These first
 * twenty slots are unconditional calls: a NULL slot is not silently ignored.
 * Retain a real call boundary: the callback receives a copy of the argument
 * area and may overwrite it without altering the wrapper caller's arguments. */
#define SERVICE(suffix, slot, form, start, end) \
    __attribute__((optimize("no-optimize-sibling-calls"))) \
    uint32_t kernel_service_##suffix(SERVICE_PARAMETERS_##form) \
    { \
        typedef uint32_t (*function)(SERVICE_PARAMETERS_##form); \
        volatile uint32_t *table = \
            (volatile uint32_t *)(uintptr_t)*(volatile uint32_t *)0x1b0e4; \
        return ((function)(uintptr_t)table[slot])(SERVICE_VALUES_##form); \
    }
#include "service_forward.def"
#undef SERVICE
