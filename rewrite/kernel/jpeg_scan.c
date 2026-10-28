/* Marker dispatcher, scan workspace setup and Huffman symbol lookup.
 * These are the original unchecked, overlapping layouts. In particular,
 * the dispatcher ignores helper errors, skips unknown markers by their raw
 * length and does not validate the three trailing SOS bytes. */
#include "jpeg_scan.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define WORD(a) (*(volatile uint16_t *)(uintptr_t)(uint32_t)(a))
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define FIELD(o) DWORD(context+(o))
#define CURRENT FIELD(0x5be0u)
#define END FIELD(0x5be4u)
typedef uint32_t (*one_argument)(uint32_t);
typedef uint32_t (*two_arguments)(uint32_t,uint32_t);

uint32_t kernel_jpeg_scan_83af27(volatile uint32_t context)
{
    FIELD(0)=0;
    FIELD(0x40b8u)=0;
    FIELD(0x40bcu)=0;
    FIELD(0x40c0u)=0;
    FIELD(0x43c4u)=0;
    FIELD(0x10u)=0;
    FIELD(0x14u)=0;
    FIELD(0x5becu)=0;
    FIELD(0x5be8u)=0;
    FIELD(0xa4u)=0;
    for(;;){
        if(CURRENT+2u>END)goto truncated;
        if(BYTE(CURRENT)!=255u)return 1;
        uint32_t marker=BYTE(CURRENT+1u);
        CURRENT=CURRENT+2u;
        if(marker==0xd8u)continue;
        if(marker==0xd9u)goto truncated;
        if(marker==0xc0u){((one_argument)0x83a919u)(context);continue;}
        if(marker==0xc4u){((one_argument)0x83ac7bu)(context);continue;}
        if(marker==0xdbu){((one_argument)0x83aae7u)(context);continue;}
        if(marker==0xddu){
            if(CURRENT+4u>END)goto truncated;
            FIELD(0xa4u)=((uint32_t)BYTE(CURRENT+2u)<<8u)|BYTE(CURRENT+3u);
            CURRENT=CURRENT+4u;
            continue;
        }
        if(marker==0xdau){
            if(CURRENT+3u>END)goto truncated;
            FIELD(0x58u)=BYTE(CURRENT+2u);
            CURRENT=CURRENT+3u;
            if(CURRENT+FIELD(0x58u)*2u>END)goto truncated;
            for(uint32_t i=0;(int32_t)i<(int32_t)FIELD(0x58u);++i){
                FIELD(0x5cu+i*4u)=BYTE(CURRENT);
                FIELD(0x74u+i*4u)=BYTE(CURRENT+1u)>>4u;
                FIELD(0x68u+i*4u)=BYTE(CURRENT+1u)&15u;
                CURRENT=CURRENT+2u;
            }
            CURRENT=CURRENT+3u;
            return 0;
        }
        if(CURRENT+2u>END)goto truncated;
        CURRENT=CURRENT+(((uint32_t)BYTE(CURRENT)<<8u)|BYTE(CURRENT+1u));
    }
truncated:
    CURRENT=END;
    return 1;
}

uint32_t kernel_jpeg_prepare_83b233(volatile uint32_t context)
{
    for(uint32_t i=0;(int32_t)i<(int32_t)FIELD(0x58u);++i){
        for(uint32_t j=0;(int32_t)j<(int32_t)FIELD(0x18u);++j){
            if(FIELD(0x5cu+i*4u)!=FIELD(0x1cu+j*4u))continue;
            FIELD(0x80u+i*4u)=FIELD(0x34u+j*4u);
            FIELD(0x8cu+i*4u)=FIELD(0x40u+j*4u);
            FIELD(0x98u+i*4u)=FIELD(0x4cu+j*4u);
            break;
        }
    }
    FIELD(8u)=FIELD(0x10u)*8u;
    FIELD(12u)=FIELD(0x14u)*8u;
    for(uint32_t i=0;i<4096u;++i)FIELD(0xa8u+i*4u)=128u;
    for(uint32_t i=0;(int32_t)i<(int32_t)FIELD(0x58u);++i)
        FIELD(0x40a8u+i*4u)=context+0xa8u+i*4096u;
    /* The last loop's count load is also the original raw EAX result. */
    return FIELD(0x58u);
}

uint32_t kernel_jpeg_symbol_83b351(volatile uint32_t context,
                                   uint32_t table_class,uint32_t table_index)
{
    uint32_t table=context+0x43c8u+table_class*0xc0cu+table_index*0x404u;
    uint32_t code=0,index=0;
    for(uint32_t length=1;length<=16u;++length){
        uint32_t bit=((two_arguments)0x83a835u)(context,1u)&0xffffu;
        code=(code*2u)|bit;
        while(BYTE(table+0x204u+index)==length){
            if(WORD(table+4u+index*2u)==code)return BYTE(table+0x304u+index);
            ++index;
        }
    }
    return UINT32_MAX;
}
