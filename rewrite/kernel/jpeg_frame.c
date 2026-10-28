/* JPEG frame marker payload at 0x83a919..0x83aae7. The segment length
 * and precision are ignored. Component arrays are not bounded to three
 * entries: writes can overlap adjacent arrays. Context, cursor and fields
 * stay live across stores; truncation consumes the remaining input. */
#include "jpeg_frame.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define CURRENT DWORD(context+0x5be0u)
#define END DWORD(context+0x5be4u)
uint32_t kernel_jpeg_frame_83a919(volatile uint32_t context)
{
    if(CURRENT+8u>END)goto truncated;
    DWORD(context+4u)=((uint32_t)BYTE(CURRENT+3u)<<8u)|BYTE(CURRENT+4u);
    DWORD(context)=((uint32_t)BYTE(CURRENT+5u)<<8u)|BYTE(CURRENT+6u);
    DWORD(context+24u)=BYTE(CURRENT+7u);
    uint32_t count=DWORD(context+24u);
    CURRENT=CURRENT+8u;
    if(CURRENT+count*3u>END)goto truncated;
    for(uint32_t i=0;(int32_t)i<(int32_t)count;++i){
        DWORD(context+0x1cu+i*4u)=BYTE(CURRENT);
        uint32_t sampling=BYTE(CURRENT+1u);
        DWORD(context+0x28u+i*4u)=sampling;
        DWORD(context+0x34u+i*4u)=sampling>>4u;
        uint32_t horizontal=DWORD(context+0x34u+i*4u);
        DWORD(context+0x40u+i*4u)=sampling&15u;
        uint32_t vertical=DWORD(context+0x40u+i*4u);
        if((int32_t)DWORD(context+16u)<(int32_t)horizontal)
            DWORD(context+16u)=horizontal;
        if((int32_t)DWORD(context+20u)<(int32_t)vertical)
            DWORD(context+20u)=vertical;
        DWORD(context+0x4cu+i*4u)=BYTE(CURRENT+2u);
        CURRENT=CURRENT+3u;
    }
    return 0;
truncated:
    CURRENT=END;
    return 1;
}
