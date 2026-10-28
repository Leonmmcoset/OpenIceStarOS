/* Quantization marker payload at 0x83aae7..0x83ac7b. The low three flag
 * bits select a table; ANY upper bit selects two-byte source strides.
 * Only the first byte of each two-byte entry is retained. A nonempty
 * payload always processes a complete table, even with fewer than 64/128
 * declared bytes. Table three aliases the maximum-table field. */
#include "jpeg_tables.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define CURRENT DWORD(context+0x5be0u)
#define END DWORD(context+0x5be4u)
uint32_t kernel_jpeg_tables_83aae7(volatile uint32_t context)
{
    if(CURRENT+2u>END)goto truncated;
    uint32_t remaining=((uint32_t)BYTE(CURRENT)<<8u)|BYTE(CURRENT+1u);
    remaining-=2u;
    CURRENT=CURRENT+2u;
    if(CURRENT+remaining>END)goto truncated;
    while((int32_t)remaining>0){
        uint32_t source=CURRENT;
        ++CURRENT;
        uint32_t flag=BYTE(source),table=flag&7u;
        --remaining;
        if((int32_t)DWORD(context+0x43c4u)<(int32_t)table)
            DWORD(context+0x43c4u)=table;
        if(flag&0xf8u){
            for(uint32_t i=0;i<64u;++i){
                DWORD(context+0x40c4u+(table*64u+i)*4u)=BYTE(CURRENT);
                CURRENT=CURRENT+2u;
            }
            remaining-=128u;
        }else{
            for(uint32_t i=0;i<64u;++i){
                /* The original INC uses the context captured before the
                 * table store, even when that store changes the argument. */
                uint32_t captured=context;
                DWORD(context+0x40c4u+(table*64u+i)*4u)=BYTE(DWORD(captured+0x5be0u));
                ++DWORD(captured+0x5be0u);
            }
            remaining-=64u;
        }
    }
    return 0;
truncated:
    CURRENT=END;
    return 1;
}
