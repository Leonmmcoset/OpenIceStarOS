/* Original cmap dispatch, 0x82ca7c..0x82cede. Formats 0, 4, 6, 12 and 13;
 * format 2 and unknown formats return zero. Preserve signed format-0/4
 * comparisons and exact indirect-array semantics, including missing delta
 * application in the original format-4 range-offset branch. */
#include "font_cmap.h"
#include "font_parse.h"
#define B(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))

int32_t kernel_font_glyph_index_82ca7c(uint32_t info, uint32_t codepoint)
{
    uint32_t data = D(info + 4u), map = D(info + 48u);
    uint16_t format = (uint16_t)kernel_font_u16_82be14(data + map);
    if (format == 0) {
        uint32_t bytes = (uint16_t)kernel_font_u16_82be14(data + map + 2u);
        if ((int32_t)(bytes - 6u) > (int32_t)codepoint)
            return B(data + map + codepoint + 6u);
        return 0;
    }
    if (format == 6) {
        uint32_t first = (uint16_t)kernel_font_u16_82be14(data + map + 6u);
        uint32_t count = (uint16_t)kernel_font_u16_82be14(data + map + 8u);
        if (codepoint >= first && codepoint < first + count)
            return (uint16_t)kernel_font_u16_82be14(data + map + 10u + (codepoint - first) * 2u);
        return 0;
    }
    if (format == 2) return 0;
    if (format == 4) {
        uint16_t segments = (uint16_t)kernel_font_u16_82be14(data + map + 6u) >> 1;
        uint16_t search_range = (uint16_t)kernel_font_u16_82be14(data + map + 8u) >> 1;
        uint16_t entry_selector = (uint16_t)kernel_font_u16_82be14(data + map + 10u);
        uint16_t range_shift = (uint16_t)kernel_font_u16_82be14(data + map + 12u) >> 1;
        uint32_t end = map + 14u, search = end;
        if ((int32_t)codepoint > 0xffff) return 0;
        if ((int32_t)(uint16_t)kernel_font_u16_82be14(data + search + range_shift * 2u) <= (int32_t)codepoint)
            search += range_shift * 2u;
        search -= 2u;
        while (entry_selector) {
            search_range >>= 1;
            uint16_t value = (uint16_t)kernel_font_u16_82be14(data + search + search_range * 2u);
            if ((int32_t)value < (int32_t)codepoint) search += search_range * 2u;
            --entry_selector;
        }
        search += 2u;
        uint16_t item = (uint16_t)((search - end) >> 1);
        uint16_t first = (uint16_t)kernel_font_u16_82be14(data + map + 16u + segments * 2u + item * 2u);
        uint16_t last = (uint16_t)kernel_font_u16_82be14(data + end + item * 2u);
        if ((int32_t)first > (int32_t)codepoint || (int32_t)last < (int32_t)codepoint) return 0;
        uint16_t offset = (uint16_t)kernel_font_u16_82be14(data + map + 16u + segments * 6u + item * 2u);
        if (!offset) {
            int32_t delta = kernel_font_s16_82be34(data + map + 16u + segments * 4u + item * 2u);
            return (uint16_t)(codepoint + (uint32_t)delta);
        }
        return (uint16_t)kernel_font_u16_82be14(data + map + 16u + segments * 6u + item * 2u
                                              + offset + (codepoint - first) * 2u);
    }
    if (format == 12 || format == 13) {
        uint32_t count = kernel_font_u32_82be52(data + map + 12u);
        int32_t low = 0, high = (int32_t)count;
        while (low < high) {
            uint32_t mid = (uint32_t)(low + ((high - low) >> 1));
            uint32_t record = data + map + 16u + mid * 12u;
            uint32_t first = kernel_font_u32_82be52(record);
            uint32_t last = kernel_font_u32_82be52(record + 4u);
            if (codepoint < first) high = (int32_t)mid;
            else if (codepoint > last) low = (int32_t)(mid + 1u);
            else {
                uint32_t glyph = kernel_font_u32_82be52(record + 8u);
                return (int32_t)(format == 12 ? glyph + codepoint - first : glyph);
            }
        }
    }
    return 0;
}
