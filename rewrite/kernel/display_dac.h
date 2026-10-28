#ifndef OPENICESTAR_KERNEL_DISPLAY_DAC_H
#define OPENICESTAR_KERNEL_DISPLAY_DAC_H
#include <stdint.h>

/* Original 0x81ad6e: inclusive signed index range, 8-bit RGB input. */
void kernel_display_write_palette(uint32_t first, uint32_t last, uint8_t *rgb);

#endif
