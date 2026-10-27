#ifndef ICESTAR_KERNEL_SERVICE_OPTIONAL_H
#define ICESTAR_KERNEL_SERVICE_OPTIONAL_H
#include <stdint.h>

/* Names record verified table slots; application-level meanings are not yet
 * established. Return the observable EAX, including the slot address left by
 * the original void-like wrappers when no callback is installed. */
#define OPTIONAL_PARAMETERS_0 void
#define OPTIONAL_PARAMETERS_1 uint32_t a0
#define OPTIONAL_PARAMETERS_2 OPTIONAL_PARAMETERS_1, uint32_t a1
#define OPTIONAL_PARAMETERS_3 OPTIONAL_PARAMETERS_2, uint32_t a2
#define OPTIONAL_PARAMETERS_6 OPTIONAL_PARAMETERS_3, uint32_t a3, uint32_t a4, uint32_t a5
#define OPTIONAL_PARAMETERS_3W02 OPTIONAL_PARAMETERS_3
#define OPTIONAL_PARAMETERS_6W12B5 OPTIONAL_PARAMETERS_6
#define OPTIONAL_PARAMETERS_2W1 OPTIONAL_PARAMETERS_2
#define OPTIONAL_PARAMETERS_3W2 OPTIONAL_PARAMETERS_3

#define OPTIONAL(suffix, slot, form, empty, start, end) \
    uint32_t kernel_optional_##suffix(OPTIONAL_PARAMETERS_##form);
#include "service_optional.def"
#undef OPTIONAL
#endif
