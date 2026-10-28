/* DHT payload at 0x83ac7b..0x83af27. Table selection accepts both complete
 * flag nibbles. Length/code/symbol arrays deliberately retain their legacy
 * overlap and unchecked length-run sentinel. Code generation precedes the
 * symbols bounds check. All source/context accesses remain live; the table
 * identity is captured once per DHT record. */
#include "jpeg_huffman.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define WORD(a) (*(volatile uint16_t *)(uintptr_t)(uint32_t)(a))
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define CURRENT DWORD(context+0x5be0u)
#define END DWORD(context+0x5be4u)
uint32_t kernel_jpeg_huffman_83ac7b(volatile uint32_t context)
{
    if(CURRENT+2u>END)goto truncated;
    uint32_t remaining=((uint32_t)BYTE(CURRENT)<<8u)|BYTE(CURRENT+1u);
    remaining-=2u;
    CURRENT=CURRENT+2u;
    while((int32_t)remaining>0){
        if(CURRENT+17u>END)goto truncated;
        uint32_t flag=BYTE(CURRENT);
        uint32_t table=context+0x43c8u+(flag>>4u)*0xc0cu+(flag&15u)*0x404u;
        uint32_t total=0,index=0;
        for(uint32_t length=1;length<=16u;++length){
            uint32_t count=BYTE(CURRENT+length);
            total+=count;
            for(uint32_t j=0;j<count;++j)BYTE(table+0x204u+index++)=(uint8_t)length;
        }
        DWORD(table)=total;
        CURRENT=CURRENT+17u;
        index=0;
        uint32_t code=0,length=BYTE(table+0x204u);
        while((int32_t)index<(int32_t)total){
            while(BYTE(table+0x204u+index)==length){
                WORD(table+4u+index*2u)=(uint16_t)code;
                ++code;++index;
            }
            if((int32_t)index>=(int32_t)total)break;
            do{code*=2u;++length;}while(BYTE(table+0x204u+index)!=length);
        }
        if(CURRENT+total>END)goto truncated;
        for(index=0;(int32_t)index<(int32_t)total;++index)
            BYTE(table+0x304u+index)=BYTE(CURRENT+index);
        CURRENT=CURRENT+total;
        remaining-=total+18u;
    }
    return 0;
truncated:
    CURRENT=END;
    return 1;
}
