#ifndef OPENICESTAR_KERNEL_FONT_PARSE_H
#define OPENICESTAR_KERNEL_FONT_PARSE_H
#include <stdint.h>

/* i386 SysV: a hidden output pointer precedes ordinary arguments; the
 * callee removes only that pointer with RET 4. INDEX GET takes a by-value
 * buffer and advances its copy in the caller's argument area. */
typedef struct {
    uint32_t data;
    int32_t cursor;
    int32_t size;
} KernelFontBuffer;

uint32_t kernel_font_get8_82b838(uint32_t buffer);
uint32_t kernel_font_peek8_82b86e(uint32_t buffer);
void kernel_font_seek_82b8a2(uint32_t buffer, int32_t offset);
void kernel_font_skip_82b8de(uint32_t buffer, int32_t count);
uint32_t kernel_font_getn_82b8fe(uint32_t buffer, int32_t count);
KernelFontBuffer kernel_font_buffer_82b946(uint32_t data, int32_t size);
KernelFontBuffer kernel_font_range_82b962(uint32_t buffer, int32_t start, int32_t size);
KernelFontBuffer kernel_font_cff_index_82b9ca(uint32_t buffer);
uint32_t kernel_font_cff_int_82ba70(uint32_t buffer);
void kernel_font_cff_skip_82bb70(uint32_t buffer);
KernelFontBuffer kernel_font_cff_dict_82bbe4(uint32_t buffer, uint32_t key);
void kernel_font_cff_ints_82bcc8(uint32_t buffer, uint32_t key, int32_t count, uint32_t output);
uint32_t kernel_font_cff_count_82bd28(uint32_t buffer);
KernelFontBuffer kernel_font_cff_item_82bd56(KernelFontBuffer buffer, int32_t index);
uint32_t kernel_font_u16_82be14(uint32_t data);
int32_t kernel_font_s16_82be34(uint32_t data);
uint32_t kernel_font_u32_82be52(uint32_t data);
int32_t kernel_font_s32_82be86(uint32_t data);
uint32_t kernel_font_signature_82beba(uint32_t data);
uint32_t kernel_font_table_82bfe6(uint32_t data, uint32_t offset, uint32_t tag);
int32_t kernel_font_offset_82c0b2(uint32_t data, int32_t index);
int32_t kernel_font_count_82c196(uint32_t data);
KernelFontBuffer kernel_font_cff_subrs_82c232(KernelFontBuffer font, KernelFontBuffer dictionary);
uint32_t kernel_font_svg_82c34c(uint32_t info);
#endif
