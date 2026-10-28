#ifndef OPENICESTAR_KERNEL_ICON_DIB_H
#define OPENICESTAR_KERNEL_ICON_DIB_H
#include <stdint.h>

/* Unchecked DIB payload renderer, originally 0x82b20c..0x82b678. */
void kernel_icon_dib_82b20c(uint32_t surface, uint32_t x, uint32_t y,
                           uint32_t dib);
#endif
