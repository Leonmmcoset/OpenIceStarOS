#include "service_optional.h"

#define OPTIONAL_VALUES_0
#define OPTIONAL_VALUES_1 a0
#define OPTIONAL_VALUES_2 a0, a1
#define OPTIONAL_VALUES_3W02 (uint32_t)(uint16_t)a0, a1, (uint32_t)(uint16_t)a2
#define OPTIONAL_VALUES_6W12B5 a0, (uint32_t)(uint16_t)a1, (uint32_t)(uint16_t)a2, a3, a4, (uint32_t)(uint8_t)a5
#define OPTIONAL_VALUES_2W1 a0, (uint32_t)(uint16_t)a1
#define OPTIONAL_VALUES_3W2 a0, a1, (uint32_t)(uint16_t)a2
#define OPTIONAL_EMPTY_Z(table, slot) 0u
#define OPTIONAL_EMPTY_A(table, slot) ((uint32_t)(uintptr_t)&table[slot])

/* The original captures the global table once, tests a slot, then loads that
 * same slot again for the call. Both reads remain volatile. A real call also
 * isolates the wrapper caller's argument area from callback writes. Omitting
 * the frame pointer keeps slot 32 inside its original 44-byte interval. */
#define OPTIONAL(suffix, slot, form, empty, start, end) \
    __attribute__((optimize("no-optimize-sibling-calls", "omit-frame-pointer"))) \
    uint32_t kernel_optional_##suffix(OPTIONAL_PARAMETERS_##form) \
    { \
        typedef uint32_t (*function)(OPTIONAL_PARAMETERS_##form); \
        volatile uint32_t *table = \
            (volatile uint32_t *)(uintptr_t)*(volatile uint32_t *)0x1b0e4; \
        if (table[slot]) \
            return ((function)(uintptr_t)table[slot])(OPTIONAL_VALUES_##form); \
        return OPTIONAL_EMPTY_##empty(table, slot); \
    }
#include "service_optional.def"
#undef OPTIONAL
