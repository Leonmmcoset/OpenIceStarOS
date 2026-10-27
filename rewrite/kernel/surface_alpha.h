#ifndef ICESTAR_KERNEL_SURFACE_ALPHA_H
#define ICESTAR_KERNEL_SURFACE_ALPHA_H
#include <stdint.h>

/* Original 0x818134: destination, source and four signed DWORD weights. */
uint32_t kernel_surface_blend4(uint32_t destination, uint32_t source,
    uint32_t weights);
#endif
