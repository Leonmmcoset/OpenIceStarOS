/* Original SFNT/TrueType/CFF initializer. Preserve partially initialized
 * fields on failure, the large CFF buffer bound, CID dictionaries and the
 * last eligible Unicode cmap record selection policy. */
#include "font_init.h"
#include "font_parse.h"
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define P(a) ((uint32_t)(uintptr_t)(a))
#define BUFFER(a) (*(KernelFontBuffer *)(uintptr_t)(uint32_t)(a))

int32_t kernel_font_init_82c3bc(uint32_t info, uint32_t data, uint32_t offset)
{
    D(info + 4u) = data;
    D(info + 8u) = offset;
    BUFFER(info + 56u) = kernel_font_buffer_82b946(0, 0);
    uint32_t cmap = kernel_font_table_82bfe6(data, offset, 0x83cf98u);
    D(info + 16u) = kernel_font_table_82bfe6(data, offset, 0x83cf9du);
    D(info + 20u) = kernel_font_table_82bfe6(data, offset, 0x83cfa2u);
    D(info + 24u) = kernel_font_table_82bfe6(data, offset, 0x83cfa7u);
    D(info + 28u) = kernel_font_table_82bfe6(data, offset, 0x83cfacu);
    D(info + 32u) = kernel_font_table_82bfe6(data, offset, 0x83cfb1u);
    D(info + 36u) = kernel_font_table_82bfe6(data, offset, 0x83cfb6u);
    D(info + 40u) = kernel_font_table_82bfe6(data, offset, 0x83cfbbu);
    if (!cmap || !D(info + 20u) || !D(info + 28u) || !D(info + 32u)) return 0;
    if (D(info + 24u)) {
        if (!D(info + 16u)) return 0;
    } else {
        uint32_t charstring_type = 2, charstrings = 0, fdarray = 0, fdselect = 0;
        uint32_t cff = kernel_font_table_82bfe6(data, offset, 0x83cfc0u);
        if (!cff) return 0;
        BUFFER(info + 104u) = kernel_font_buffer_82b946(0, 0);
        BUFFER(info + 116u) = kernel_font_buffer_82b946(0, 0);
        BUFFER(info + 56u) = kernel_font_buffer_82b946(data + cff, 0x20000000);
        KernelFontBuffer buffer = BUFFER(info + 56u);
        kernel_font_skip_82b8de(P(&buffer), 2);
        uint32_t header = (uint8_t)kernel_font_get8_82b838(P(&buffer));
        kernel_font_seek_82b8a2(P(&buffer), (int32_t)header);
        (void)kernel_font_cff_index_82b9ca(P(&buffer));
        KernelFontBuffer top_index = kernel_font_cff_index_82b9ca(P(&buffer));
        KernelFontBuffer top = kernel_font_cff_item_82bd56(top_index, 0);
        (void)kernel_font_cff_index_82b9ca(P(&buffer));
        BUFFER(info + 80u) = kernel_font_cff_index_82b9ca(P(&buffer));
        kernel_font_cff_ints_82bcc8(P(&top), 17, 1, P(&charstrings));
        kernel_font_cff_ints_82bcc8(P(&top), 0x106, 1, P(&charstring_type));
        kernel_font_cff_ints_82bcc8(P(&top), 0x124, 1, P(&fdarray));
        kernel_font_cff_ints_82bcc8(P(&top), 0x125, 1, P(&fdselect));
        BUFFER(info + 92u) = kernel_font_cff_subrs_82c232(buffer, top);
        if (charstring_type != 2 || !charstrings) return 0;
        if (fdarray) {
            if (!fdselect) return 0;
            kernel_font_seek_82b8a2(P(&buffer), (int32_t)fdarray);
            BUFFER(info + 104u) = kernel_font_cff_index_82b9ca(P(&buffer));
            BUFFER(info + 116u) = kernel_font_range_82b962(P(&buffer), (int32_t)fdselect,
                (int32_t)((uint32_t)buffer.size - fdselect));
        }
        kernel_font_seek_82b8a2(P(&buffer), (int32_t)charstrings);
        BUFFER(info + 68u) = kernel_font_cff_index_82b9ca(P(&buffer));
    }
    uint32_t maxp = kernel_font_table_82bfe6(data, offset, 0x83cfc5u);
    D(info + 12u) = maxp ? (uint16_t)kernel_font_u16_82be14(data + maxp + 4u) : 0xffffu;
    D(info + 44u) = 0xffffffffu;
    uint32_t count = (uint16_t)kernel_font_u16_82be14(data + cmap + 2u);
    D(info + 48u) = 0;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t record = cmap + 4u + i * 8u;
        uint16_t platform = (uint16_t)kernel_font_u16_82be14(data + record);
        if (platform == 0) {
            D(info + 48u) = cmap + kernel_font_u32_82be52(data + record + 4u);
        } else if (platform == 3) {
            uint16_t encoding = (uint16_t)kernel_font_u16_82be14(data + record + 2u);
            if (encoding == 1 || encoding == 10)
                D(info + 48u) = cmap + kernel_font_u32_82be52(data + record + 4u);
        }
    }
    if (!D(info + 48u)) return 0;
    D(info + 52u) = (uint16_t)kernel_font_u16_82be14(data + D(info + 20u) + 50u);
    return 1;
}
