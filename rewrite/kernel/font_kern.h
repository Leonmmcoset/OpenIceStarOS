#ifndef OPENICESTAR_KERNEL_FONT_KERN_H
#define OPENICESTAR_KERNEL_FONT_KERN_H
#include <stdint.h>
int32_t kernel_font_kern_length_82fa50(uint32_t info);
int32_t kernel_font_kern_table_82faca(uint32_t info, uint32_t output, int32_t capacity);
int32_t kernel_font_legacy_kern_82fc1a(uint32_t info, uint32_t left, uint32_t right);
int32_t kernel_font_coverage_82fd28(uint32_t table, int32_t glyph);
int32_t kernel_font_class_82febe(uint32_t table, int32_t glyph);
int32_t kernel_font_gpos_kern_830024(uint32_t info, int32_t left, int32_t right);
int32_t kernel_font_glyph_kern_830442(uint32_t info, int32_t left, int32_t right);
int32_t kernel_font_codepoint_kern_8304a8(uint32_t info, uint32_t left, uint32_t right);
void kernel_font_codepoint_metrics_830510(uint32_t info, uint32_t codepoint, uint32_t advance, uint32_t bearing);
#endif
