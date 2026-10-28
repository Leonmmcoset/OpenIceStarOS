#ifndef OPENICESTAR_KERNEL_DISPLAY_PALETTE_H
#define OPENICESTAR_KERNEL_DISPLAY_PALETTE_H
#include <stdint.h>

void kernel_display_init_palette(void);
/* Original entry 0x81afbc fills the software color lookup table. */
void kernel_display_select_depth(uint32_t depth);

#endif
