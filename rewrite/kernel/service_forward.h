#ifndef ICESTAR_KERNEL_SERVICE_FORWARD_H
#define ICESTAR_KERNEL_SERVICE_FORWARD_H

#include <stdint.h>

/* The slot number is the verified name; higher-level parameter meanings are
 * not yet established for every entry. Each original wrapper returns the
 * callback's raw EAX. Slots 6 and 12 sign-extend their last byte argument. */
#define SERVICE_PARAMETERS_1 uint32_t a0
#define SERVICE_PARAMETERS_3 SERVICE_PARAMETERS_1, uint32_t a1, uint32_t a2
#define SERVICE_PARAMETERS_4 SERVICE_PARAMETERS_3, uint32_t a3
#define SERVICE_PARAMETERS_5 SERVICE_PARAMETERS_4, uint32_t a4
#define SERVICE_PARAMETERS_6 SERVICE_PARAMETERS_5, uint32_t a5
#define SERVICE_PARAMETERS_7 SERVICE_PARAMETERS_6, uint32_t a6
#define SERVICE_PARAMETERS_6S SERVICE_PARAMETERS_6

#define SERVICE(suffix, slot, form, start, end) \
    uint32_t kernel_service_##suffix(SERVICE_PARAMETERS_##form);
#include "service_forward.def"
#undef SERVICE

#endif
