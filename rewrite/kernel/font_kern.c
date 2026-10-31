/* Original legacy kern and GPOS pair-positioning policy. GPOS takes priority
 * whenever present, scans all lookup records without feature filtering, and
 * only supports xAdvance on the first glyph (value formats 4,0). */
#include "font_kern.h"
#include "font_parse.h"
#include "font_metrics.h"
#include "font_cmap.h"
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define U16(a) ((uint16_t)kernel_font_u16_82be14(a))
#define S16(a) ((int16_t)kernel_font_s16_82be34(a))

int32_t kernel_font_kern_length_82fa50(uint32_t info)
{
    uint32_t table = D(info + 4u) + D(info + 36u);
    if (!D(info + 36u) || !U16(table + 2u) || U16(table + 8u) != 1) return 0;
    return U16(table + 10u);
}

int32_t kernel_font_kern_table_82faca(uint32_t info, uint32_t output, int32_t capacity)
{
    uint32_t table = D(info + 4u) + D(info + 36u);
    if (!D(info + 36u) || !U16(table + 2u) || U16(table + 8u) != 1) return 0;
    int32_t count = U16(table + 10u);
    if (capacity < count) count = capacity;
    for (int32_t i = 0; i < count; ++i) {
        uint32_t source = table + 18u + (uint32_t)i * 6u;
        uint32_t target = output + (uint32_t)i * 12u;
        D(target) = U16(source);
        D(target + 4u) = U16(source + 2u);
        D(target + 8u) = (uint32_t)S16(source + 4u);
    }
    return count;
}

int32_t kernel_font_legacy_kern_82fc1a(uint32_t info, uint32_t left, uint32_t right)
{
    uint32_t table = D(info + 4u) + D(info + 36u);
    if (!D(info + 36u) || !U16(table + 2u) || U16(table + 8u) != 1) return 0;
    int32_t low = 0, high = (int32_t)U16(table + 10u) - 1;
    uint32_t key = (left << 16) | right;
    while (low <= high) {
        int32_t mid = (low + high) >> 1;
        uint32_t record = table + 18u + (uint32_t)mid * 6u;
        uint32_t found = kernel_font_u32_82be52(record);
        if (key < found) high = mid - 1;
        else if (key > found) low = mid + 1;
        else return S16(record + 4u);
    }
    return 0;
}

int32_t kernel_font_coverage_82fd28(uint32_t table, int32_t glyph)
{
    uint16_t format = U16(table);
    if (format != 1 && format != 2) return -1;
    int32_t low = 0, high = (int32_t)U16(table + 2u) - 1;
    while (low <= high) {
        int32_t mid = (low + high) >> 1;
        uint32_t record = table + 4u + (uint32_t)mid * (format == 1 ? 2u : 6u);
        int32_t first = U16(record);
        int32_t last = format == 1 ? first : U16(record + 2u);
        if (glyph < first) high = mid - 1;
        else if (glyph > last) low = mid + 1;
        else return format == 1 ? mid : (int32_t)U16(record + 4u) + glyph - first;
    }
    return -1;
}

int32_t kernel_font_class_82febe(uint32_t table, int32_t glyph)
{
    uint16_t format = U16(table);
    if (format == 1) {
        int32_t first = U16(table + 2u), count = U16(table + 4u);
        if (glyph >= first && glyph < first + count)
            return U16(table + 6u + (uint32_t)(glyph - first) * 2u);
        return 0;
    }
    if (format != 2) return -1;
    int32_t low = 0, high = (int32_t)U16(table + 2u) - 1;
    while (low <= high) {
        int32_t mid = (low + high) >> 1;
        uint32_t record = table + 4u + (uint32_t)mid * 6u;
        int32_t first = U16(record), last = U16(record + 2u);
        if (glyph < first) high = mid - 1;
        else if (glyph > last) low = mid + 1;
        else return U16(record + 4u);
    }
    return 0;
}

int32_t kernel_font_gpos_kern_830024(uint32_t info, int32_t left, int32_t right)
{
    if (!D(info + 40u)) return 0;
    uint32_t table = D(info + 4u) + D(info + 40u);
    if (U16(table) != 1 || U16(table + 2u) != 0) return 0;
    uint32_t list = table + U16(table + 8u);
    uint16_t count = U16(list);
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t lookup = list + U16(list + 2u + i * 2u);
        uint16_t type = U16(lookup), subtables = U16(lookup + 4u);
        if (type != 2) continue;
        for (uint32_t j = 0; j < subtables; ++j) {
            uint32_t sub = lookup + U16(lookup + 6u + j * 2u);
            uint16_t format = U16(sub), coverage = U16(sub + 2u);
            int32_t index = kernel_font_coverage_82fd28(sub + coverage, left);
            if (index == -1) continue;
            if (format == 1) {
                uint16_t value1 = U16(sub + 4u), value2 = U16(sub + 6u);
                if (value1 != 4 || value2 != 0) return 0;
                uint16_t pairs = U16(sub + 8u);
                uint32_t set = sub + U16(sub + 10u + (uint32_t)index * 2u);
                uint16_t set_count = U16(set);
                if ((int32_t)pairs <= index) return 0;
                int32_t low = 0, high = (int32_t)set_count - 1;
                while (low <= high) {
                    int32_t mid = (low + high) >> 1;
                    uint32_t record = set + 2u + (uint32_t)mid * 4u;
                    int32_t glyph = U16(record);
                    if (right < glyph) high = mid - 1;
                    else if (right > glyph) low = mid + 1;
                    else return S16(record + 2u);
                }
            } else if (format == 2) {
                uint16_t value1 = U16(sub + 4u), value2 = U16(sub + 6u);
                if (value1 != 4 || value2 != 0) return 0;
                uint16_t class1_offset = U16(sub + 8u), class2_offset = U16(sub + 10u);
                int32_t class1 = kernel_font_class_82febe(sub + class1_offset, left);
                int32_t class2 = kernel_font_class_82febe(sub + class2_offset, right);
                uint16_t count1 = U16(sub + 12u), count2 = U16(sub + 14u);
                if (class1 < 0 || class1 >= count1 || class2 < 0 || class2 >= count2) return 0;
                return S16(sub + 16u + ((uint32_t)class1 * count2 + (uint32_t)class2) * 2u);
            } else return 0;
        }
    }
    return 0;
}

int32_t kernel_font_glyph_kern_830442(uint32_t info, int32_t left, int32_t right)
{
    if (D(info + 40u)) return kernel_font_gpos_kern_830024(info, left, right);
    if (D(info + 36u)) return kernel_font_legacy_kern_82fc1a(info, (uint32_t)left, (uint32_t)right);
    return 0;
}

int32_t kernel_font_codepoint_kern_8304a8(uint32_t info, uint32_t left, uint32_t right)
{
    if (!D(info + 36u) && !D(info + 40u)) return 0;
    int32_t glyph_right = kernel_font_glyph_index_82ca7c(info, right);
    int32_t glyph_left = kernel_font_glyph_index_82ca7c(info, left);
    return kernel_font_glyph_kern_830442(info, glyph_left, glyph_right);
}

void kernel_font_codepoint_metrics_830510(uint32_t info, uint32_t codepoint, uint32_t advance, uint32_t bearing)
{
    int32_t glyph = kernel_font_glyph_index_82ca7c(info, codepoint);
    kernel_font_hmetrics_82f958(info, glyph, advance, bearing);
}
