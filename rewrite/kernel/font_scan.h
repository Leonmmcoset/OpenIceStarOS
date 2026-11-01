#ifndef OPENICESTAR_KERNEL_FONT_SCAN_H
#define OPENICESTAR_KERNEL_FONT_SCAN_H
#include <stdint.h>
uint32_t kernel_font_active_830c5a(uint32_t heap, uint32_t edge, int32_t offset_x, float start_y, uint32_t userdata);
void kernel_font_fill_scanline_830dc4(uint32_t scanline, int32_t width, uint32_t active, int32_t weight);
void kernel_font_edge_insertion_831340(uint32_t edges, int32_t count);
void kernel_font_edge_quicksort_831480(uint32_t edges, int32_t count);
void kernel_font_edge_sort_831858(uint32_t edges, int32_t count);
#endif
