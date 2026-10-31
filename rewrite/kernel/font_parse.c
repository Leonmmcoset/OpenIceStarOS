/* CFF buffer/dictionary and SFNT header helpers, 0x82b838..0x82c3bc.
 * Preserve original signed bounds, wrap arithmetic, aliasing, byte order
 * and collection-index behavior rather than imposing a new font policy. */
#include "font_parse.h"
#define B(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define CURSOR(a) ((int32_t)D((a) + 4u))
#define SIZE(a) ((int32_t)D((a) + 8u))
/* The explicit address is needed for the range helper's early writes when
 * the hidden return buffer aliases its input. GCC materializes an EBP frame
 * for this builtin; [EBP+8] is the i386 hidden aggregate-return pointer. */
#define RETURN_BUFFER() (((uint32_t *)__builtin_frame_address(0))[2])

uint32_t kernel_font_get8_82b838(uint32_t buffer)
{
    if (CURSOR(buffer) >= SIZE(buffer)) return 0;
    uint32_t value = B(D(buffer) + D(buffer + 4u));
    D(buffer + 4u) += 1u;
    return value;
}

uint32_t kernel_font_peek8_82b86e(uint32_t buffer)
{
    if (CURSOR(buffer) >= SIZE(buffer)) return 0;
    return B(D(buffer) + D(buffer + 4u));
}

void kernel_font_seek_82b8a2(uint32_t buffer, int32_t offset)
{
    int32_t size = SIZE(buffer);
    D(buffer + 4u) = (size < offset || offset < 0) ? D(buffer + 8u) : (uint32_t)offset;
}

void kernel_font_skip_82b8de(uint32_t buffer, int32_t count)
{
    kernel_font_seek_82b8a2(buffer, (int32_t)(D(buffer + 4u) + (uint32_t)count));
}

uint32_t kernel_font_getn_82b8fe(uint32_t buffer, int32_t count)
{
    uint32_t value = 0;
    for (uint32_t i = 0; (int32_t)i < count; ++i)
        value = (value << 8) | (uint8_t)kernel_font_get8_82b838(buffer);
    return value;
}

KernelFontBuffer kernel_font_buffer_82b946(uint32_t data, int32_t size)
{
    return (KernelFontBuffer){data, 0, size};
}

KernelFontBuffer kernel_font_range_82b962(uint32_t buffer, int32_t start, int32_t size)
{
    uint32_t output = RETURN_BUFFER();
    D(output) = 0;
    D(output + 8u) = 0;
    D(output + 4u) = 0;
    if (start >= 0 && size >= 0 && SIZE(buffer) >= start &&
        (int32_t)(D(buffer + 8u) - (uint32_t)start) >= size) {
        D(output) = D(buffer) + (uint32_t)start;
        D(output + 8u) = (uint32_t)size;
    }
    return *(KernelFontBuffer *)(uintptr_t)output;
}

KernelFontBuffer kernel_font_cff_index_82b9ca(uint32_t buffer)
{
    uint32_t start = D(buffer + 4u);
    uint32_t count = kernel_font_getn_82b8fe(buffer, 2);
    if (count) {
        uint32_t width = (uint8_t)kernel_font_get8_82b838(buffer);
        kernel_font_skip_82b8de(buffer, (int32_t)(width * count));
        uint32_t last = kernel_font_getn_82b8fe(buffer, (int32_t)width);
        kernel_font_skip_82b8de(buffer, (int32_t)(last - 1u));
    }
    return kernel_font_range_82b962(buffer, (int32_t)start,
                                   (int32_t)(D(buffer + 4u) - start));
}

uint32_t kernel_font_cff_int_82ba70(uint32_t buffer)
{
    uint32_t first = (uint8_t)kernel_font_get8_82b838(buffer);
    if (first >= 32u && first <= 246u) return first - 139u;
    if (first >= 247u && first <= 250u)
        return (first << 8) + (uint8_t)kernel_font_get8_82b838(buffer) - 0xf694u;
    if (first >= 251u && first <= 254u)
        return ((251u - first) << 8) - (uint8_t)kernel_font_get8_82b838(buffer) - 108u;
    if (first == 28u) return kernel_font_getn_82b8fe(buffer, 2);
    if (first == 29u) return kernel_font_getn_82b8fe(buffer, 4);
    return 0;
}

void kernel_font_cff_skip_82bb70(uint32_t buffer)
{
    if ((uint8_t)kernel_font_peek8_82b86e(buffer) == 30u) {
        kernel_font_skip_82b8de(buffer, 1);
        while (CURSOR(buffer) < SIZE(buffer)) {
            uint32_t value = (uint8_t)kernel_font_get8_82b838(buffer);
            if ((value & 15u) == 15u || (value >> 4) == 15u) break;
        }
    } else {
        (void)kernel_font_cff_int_82ba70(buffer);
    }
}

KernelFontBuffer kernel_font_cff_dict_82bbe4(uint32_t buffer, uint32_t key)
{
    kernel_font_seek_82b8a2(buffer, 0);
    while (CURSOR(buffer) < SIZE(buffer)) {
        uint32_t start = D(buffer + 4u);
        while ((uint8_t)kernel_font_peek8_82b86e(buffer) > 27u)
            kernel_font_cff_skip_82bb70(buffer);
        uint32_t end = D(buffer + 4u);
        uint32_t operation = (uint8_t)kernel_font_get8_82b838(buffer);
        if (operation == 12u) operation = (uint8_t)kernel_font_get8_82b838(buffer) | 0x100u;
        if (operation == key)
            return kernel_font_range_82b962(buffer, (int32_t)start, (int32_t)(end - start));
    }
    return kernel_font_range_82b962(buffer, 0, 0);
}

void kernel_font_cff_ints_82bcc8(uint32_t buffer, uint32_t key, int32_t count, uint32_t output)
{
    KernelFontBuffer operands = kernel_font_cff_dict_82bbe4(buffer, key);
    uint32_t local = (uint32_t)(uintptr_t)&operands;
    for (uint32_t i = 0; (int32_t)i < count && operands.cursor < operands.size; ++i)
        D(output + i * 4u) = kernel_font_cff_int_82ba70(local);
}

uint32_t kernel_font_cff_count_82bd28(uint32_t buffer)
{
    kernel_font_seek_82b8a2(buffer, 0);
    return kernel_font_getn_82b8fe(buffer, 2);
}

KernelFontBuffer kernel_font_cff_item_82bd56(KernelFontBuffer buffer, int32_t index)
{
    uint32_t local = (uint32_t)(uintptr_t)&buffer;
    kernel_font_seek_82b8a2(local, 0);
    uint32_t count = kernel_font_getn_82b8fe(local, 2);
    uint32_t width = (uint8_t)kernel_font_get8_82b838(local);
    kernel_font_skip_82b8de(local, (int32_t)((uint32_t)index * width));
    uint32_t start = kernel_font_getn_82b8fe(local, (int32_t)width);
    uint32_t end = kernel_font_getn_82b8fe(local, (int32_t)width);
    return kernel_font_range_82b962(local, (int32_t)((count + 1u) * width + start + 2u),
                                   (int32_t)(end - start));
}

uint32_t kernel_font_u16_82be14(uint32_t data)
{
    return (uint16_t)(((uint32_t)B(data) << 8) + B(data + 1u));
}

int32_t kernel_font_s16_82be34(uint32_t data)
{
    return (int16_t)(((uint32_t)B(data) << 8) + B(data + 1u));
}

uint32_t kernel_font_u32_82be52(uint32_t data)
{
    uint32_t value = (uint32_t)B(data) << 24;
    value += (uint32_t)B(data + 1u) << 16;
    value += (uint32_t)B(data + 2u) << 8;
    return value + B(data + 3u);
}

int32_t kernel_font_s32_82be86(uint32_t data)
{
    uint32_t value = (uint32_t)B(data) << 24;
    value += (uint32_t)B(data + 1u) << 16;
    value += (uint32_t)B(data + 2u) << 8;
    return (int32_t)(value + B(data + 3u));
}

#define TAG(data,a,b,c,d) (B(data)==(a) && B((data)+1u)==(b) && B((data)+2u)==(c) && B((data)+3u)==(d))

uint32_t kernel_font_signature_82beba(uint32_t data)
{
    return TAG(data,'1',0,0,0) || TAG(data,'t','y','p','1') || TAG(data,'O','T','T','O') ||
           TAG(data,0,1,0,0) || TAG(data,'t','r','u','e');
}

uint32_t kernel_font_table_82bfe6(uint32_t data, uint32_t offset, uint32_t tag)
{
    uint32_t count = (uint16_t)kernel_font_u16_82be14(data + offset + 4u);
    uint32_t directory = offset + 12u;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t record = directory + (i << 4);
        if ((int32_t)B(data + record) == (int8_t)B(tag) &&
            (int32_t)B(data + record + 1u) == (int8_t)B(tag + 1u) &&
            (int32_t)B(data + record + 2u) == (int8_t)B(tag + 2u) &&
            (int32_t)B(data + record + 3u) == (int8_t)B(tag + 3u))
            return kernel_font_u32_82be52(data + record + 8u);
    }
    return 0;
}

int32_t kernel_font_offset_82c0b2(uint32_t data, int32_t index)
{
    if (kernel_font_signature_82beba(data)) return index == 0 ? 0 : -1;
    if (TAG(data,'t','t','c','f')) {
        uint32_t version = kernel_font_u32_82be52(data + 4u);
        if (version == 0x10000u || kernel_font_u32_82be52(data + 4u) == 0x20000u) {
            int32_t count = kernel_font_s32_82be86(data + 8u);
            if (index >= count) return -1;
            return (int32_t)kernel_font_u32_82be52(data + 12u + ((uint32_t)index << 2));
        }
    }
    return -1;
}

int32_t kernel_font_count_82c196(uint32_t data)
{
    if (kernel_font_signature_82beba(data)) return 1;
    if (TAG(data,'t','t','c','f')) {
        uint32_t version = kernel_font_u32_82be52(data + 4u);
        if (version == 0x10000u || kernel_font_u32_82be52(data + 4u) == 0x20000u)
            return kernel_font_s32_82be86(data + 8u);
    }
    return 0;
}

KernelFontBuffer kernel_font_cff_subrs_82c232(KernelFontBuffer font, KernelFontBuffer dictionary)
{
    uint32_t private_values[2] = {0, 0}, subrs = 0;
    kernel_font_cff_ints_82bcc8((uint32_t)(uintptr_t)&dictionary, 18u, 2,
                               (uint32_t)(uintptr_t)private_values);
    if (!private_values[0] || !private_values[1]) return kernel_font_buffer_82b946(0, 0);
    uint32_t local = (uint32_t)(uintptr_t)&font;
    KernelFontBuffer private_dict = kernel_font_range_82b962(local, (int32_t)private_values[1],
                                                            (int32_t)private_values[0]);
    kernel_font_cff_ints_82bcc8((uint32_t)(uintptr_t)&private_dict, 19u, 1,
                               (uint32_t)(uintptr_t)&subrs);
    if (!subrs) return kernel_font_buffer_82b946(0, 0);
    kernel_font_seek_82b8a2(local, (int32_t)(private_values[1] + subrs));
    return kernel_font_cff_index_82b9ca(local);
}

uint32_t kernel_font_svg_82c34c(uint32_t info)
{
    if ((int32_t)D(info + 44u) < 0) {
        uint32_t table = kernel_font_table_82bfe6(D(info + 4u), D(info + 8u), 0x83cf93u);
        if (table) {
            uint32_t relative = kernel_font_u32_82be52(table + D(info + 4u) + 2u);
            D(info + 44u) = relative + table;
        } else D(info + 44u) = 0;
    }
    return D(info + 44u);
}
