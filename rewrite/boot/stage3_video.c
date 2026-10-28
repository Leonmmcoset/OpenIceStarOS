/* Real-mode VBE selection at original stage offsets 0x189..0x2c3.
 * Boot fields are DS-relative; VBE buffers/table are physical ES=0 memory.
 * Retain the 32-bit-only enumeration, sentinel 0x105, last 1024x768
 * fallback, truncated WORD table offsets and unchecked mode-set result. */
#include "stage3_video.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define WORD(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(a))
void stage_video_select(void)
{
    if(stage_video_bios(0x4f00u,0)!=0x4fu||stage_video_read(0x504u,2)<0x200u)goto fallback;
    for(uint32_t mode=0x100u;mode<0x200u;++mode){
        if(stage_video_bios(0x4f01u,mode)!=0x4fu)continue;
        if(stage_video_read(0x719u,1)!=32u||!(stage_video_read(0x700u,2)&1u))continue;
        uint32_t record=(uint16_t)((uint16_t)WORD(0x38u)*10u+0x800u);
        stage_video_write(record,mode,2);
        stage_video_write((uint16_t)(record+2u),stage_video_read(0x712u,2),2);
        stage_video_write((uint16_t)(record+4u),stage_video_read(0x714u,2),2);
        stage_video_write((uint16_t)(record+6u),stage_video_read(0x719u,1),2);
        stage_video_write((uint16_t)(record+8u),stage_video_read(0x700u,1),2);
        DWORD(0x38u)=DWORD(0x38u)+1u;
        if(WORD(0x56u)==1u)continue;
        if(WORD(0xau)==0x105u||WORD(0xau)==mode){WORD(0x56u)=1;continue;}
        if(stage_video_read((uint16_t)(record+2u),2)==1024u&&
           stage_video_read((uint16_t)(record+4u),2)==768u)WORD(0x58u)=(uint16_t)mode;
    }
    if(WORD(0x56u)!=1u)WORD(0xau)=WORD(0x58u);
    if(stage_video_bios(0x4f01u,WORD(0xau))!=0x4fu||!(stage_video_read(0x700u,2)&1u))goto fallback;
    stage_video_bios(0x4f02u,(uint16_t)(WORD(0xau)+0x4000u));
    BYTE(0x16u)=(uint8_t)stage_video_read(0x719u,1);
    WORD(0x1au)=(uint16_t)stage_video_read(0x712u,2);
    WORD(0x1eu)=(uint16_t)stage_video_read(0x714u,2);
    DWORD(0x22u)=stage_video_read(0x728u,4);
    return;
fallback:
    stage_video_bios(0x13u,0);
    DWORD(0xau)=0;
    BYTE(0x16u)=8;
    WORD(0x1au)=320;
    WORD(0x1eu)=200;
    DWORD(0x22u)=0xa0000;
}
