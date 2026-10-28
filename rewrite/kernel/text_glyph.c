/* 8x16 bitmap glyphs at 0x81db5a..0x81e106. The original signed low byte
 * admits only 0..127; negative coordinates have no lower clipping. The
 * rotated entries test unrotated coordinates but store at (x+row,y-column).
 * Capture the descriptor/font/character, but retain live geometry, colors,
 * descriptor fields and each row's fetched font byte across stores.
 */
#include "text_glyph.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define GLYPH_BODY(store,bytes,rotated) \
    int32_t glyph=(int8_t)character; \
    uint32_t surface=DWORD(0x500000u+id*4u); \
    if((int32_t)DWORD(surface+12u)<=(int32_t)x || \
       (int32_t)DWORD(surface+16u)<=(int32_t)y || glyph<0 || glyph>132)return 0; \
    uint32_t font=DWORD(0x1b020u); \
    for(uint32_t row=0;row<16;++row){ \
        uint8_t bits=BYTE(font+(uint32_t)glyph*16u+row); \
        for(uint32_t column=0;column<8;++column){ \
            if((int32_t)(x+column)>=(int32_t)DWORD(surface+12u))continue; \
            if((int32_t)(y+row)>=(int32_t)DWORD(surface+16u))continue; \
            uint32_t yy=rotated?y-column:y+row; \
            uint32_t offset=yy*DWORD(surface+12u)+x+(rotated?row:column); \
            uint32_t pixels=DWORD(surface); \
            if((bits>>(7u-column))&1u)store(pixels+offset*bytes)=foreground; \
            else store(pixels+offset*bytes)=background; \
        } \
    } \
    return 1

uint32_t kernel_text_glyph8_81db5a(uint32_t id,volatile uint32_t x,
    volatile uint32_t y,volatile uint32_t foreground,volatile uint32_t background,uint32_t character)
{GLYPH_BODY(BYTE,1u,0);}
uint32_t kernel_text_rotated8_81dcc0(uint32_t id,volatile uint32_t x,
    volatile uint32_t y,volatile uint32_t foreground,volatile uint32_t background,uint32_t character)
{GLYPH_BODY(BYTE,1u,1);}
uint32_t kernel_text_glyph32_81de2a(uint32_t id,volatile uint32_t x,
    volatile uint32_t y,volatile uint32_t foreground,volatile uint32_t background,uint32_t character)
{GLYPH_BODY(DWORD,4u,0);}
uint32_t kernel_text_rotated32_81df96(uint32_t id,volatile uint32_t x,
    volatile uint32_t y,volatile uint32_t foreground,volatile uint32_t background,uint32_t character)
{GLYPH_BODY(DWORD,4u,1);}
