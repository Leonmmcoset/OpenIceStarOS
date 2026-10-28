/* Entropy decode one block, retaining the original unchecked AC runs,
 * caller-provided zigzag bytes, predictor wrapping and captured DQT table. */
#include "jpeg_block.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
typedef uint32_t (*symbol_reader)(uint32_t,uint32_t,uint32_t);
typedef uint32_t (*magnitude_reader)(uint32_t,uint32_t);
uint32_t kernel_jpeg_block_83b7ab(volatile uint32_t context,volatile uint32_t component,
                                volatile uint32_t output,volatile uint32_t zigzag)
{
    uint32_t quant=context+0x40c4u+DWORD(context+0x98u+component*4u)*256u;
    uint32_t size=((symbol_reader)0x83b351u)(context,0,DWORD(context+0x74u+component*4u));
    if((int32_t)size<0)return 0;
    uint32_t value=((magnitude_reader)0x83b73fu)(context,size);
    DWORD(context+0x40b8u+component*4u)=value+DWORD(context+0x40b8u+component*4u);
    DWORD(output)=DWORD(context+0x40b8u+component*4u)*DWORD(quant);
    uint32_t i=1;
    while(i<=63u){
        uint32_t symbol=((symbol_reader)0x83b351u)(context,1,DWORD(context+0x68u+component*4u));
        if((int32_t)symbol<=0)break;
        uint32_t run=(symbol>>4u)&15u;
        value=((magnitude_reader)0x83b73fu)(context,symbol&15u);
        if((int32_t)value>65535)return value;
        while(run--){DWORD(output+BYTE(zigzag+i)*4u)=0;++i;}
        DWORD(output+BYTE(zigzag+i)*4u)=DWORD(quant+i*4u)*value;
        ++i;
    }
    while(i<=63u){DWORD(output+BYTE(zigzag+i)*4u)=0;++i;}
    return 0;
}
