/* Original 16x16 two-byte glyphs at 0x81e106..0x81e7b2, opaque and masked.
 * Accept both bytes in 0xa0..0xfe, including 0xa0's negative font indices.
 * A font bit is fetched again for every visible pixel, unlike the captured
 * 8x16 rows. Keep the descriptor/font offset, but keep geometry/colors and
 * descriptor fields live across stores. No lower coordinate clipping.
 */
#include "text_wide.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define WIDE_BODY(store,bytes,opaque,background_value) \
    uint32_t surface=DWORD(0x500000u+id*4u); \
    uint32_t high=BYTE(text),low=BYTE(text+1u); \
    if((int32_t)DWORD(surface+12u)<=(int32_t)x || \
       (int32_t)DWORD(surface+16u)<=(int32_t)y || \
       high<0xa0u || high==0xffu || low<0xa0u || low==0xffu)return 0; \
    high=(uint8_t)(BYTE(text)-0xa0u);low=(uint8_t)(BYTE(text+1u)-0xa0u); \
    uint32_t font=0x20200u+((high*94u+low)<<5)-0xbe0u; \
    for(uint32_t row=0;row<16;++row)for(uint32_t half=0;half<2;++half) \
      for(uint32_t bit=0;bit<8;++bit){ \
        if((int32_t)(x+half*8u+bit)>=(int32_t)DWORD(surface+12u))continue; \
        if((int32_t)(y+row)>=(int32_t)DWORD(surface+16u))continue; \
        uint32_t set=BYTE(font+row*2u+half)&(0x80u>>bit); \
        if(set || opaque){ \
            uint32_t offset=(y+row)*DWORD(surface+12u)+half*8u+bit+x; \
            uint32_t pixels=DWORD(surface); \
            if(set)store(pixels+offset*bytes)=foreground; \
            else store(pixels+offset*bytes)=background_value; \
        } \
    } \
    return 1

uint32_t kernel_text_wide8_81e106(uint32_t id,volatile uint32_t x,
    volatile uint32_t y,volatile uint32_t foreground,volatile uint32_t background,volatile uint32_t text)
{WIDE_BODY(BYTE,1u,1,background);}
uint32_t kernel_text_wide8_mask_81e2cc(uint32_t id,volatile uint32_t x,
    volatile uint32_t y,volatile uint32_t foreground,volatile uint32_t text)
{WIDE_BODY(BYTE,1u,0,0);}
uint32_t kernel_text_wide32_81e45a(uint32_t id,volatile uint32_t x,
    volatile uint32_t y,volatile uint32_t foreground,volatile uint32_t background,volatile uint32_t text)
{WIDE_BODY(DWORD,4u,1,background);}
uint32_t kernel_text_wide32_mask_81e624(uint32_t id,volatile uint32_t x,
    volatile uint32_t y,volatile uint32_t foreground,volatile uint32_t text)
{WIDE_BODY(DWORD,4u,0,0);}
