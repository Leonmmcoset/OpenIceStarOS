/* Unsigned radix rendering and digit count, 0x81eaf2..0x81ee7e.
 * Bases are unchecked: nonzero/base-zero faults and nonzero/base-one loops.
 * The byte adjustment is signed, including wrapped bytes for large bases.
 */
#include "text_number.h"
#include "text_run.h"
#define PARAMETERS volatile uint32_t id, volatile uint32_t x, volatile uint32_t y, \
    volatile uint32_t foreground, volatile uint32_t background, \
    volatile uint32_t base, volatile uint32_t number
#define NUMBER_BODY(render) \
    char text[256]; \
    uint32_t value=number, count=0; \
    if(value){ \
        while(value){value/=base; ++count;} \
        value=number; text[count]=0; --count; \
        while(value && (int32_t)count>=0){ \
            uint8_t digit=(uint8_t)(value%base+48u); \
            if((int8_t)digit>57)digit=(uint8_t)(digit+7u); \
            text[count]=(char)digit; --count; value/=base; \
        } \
    }else{text[0]='0';text[1]=0;} \
    return render(id,x,y,foreground,background,(uint32_t)(uintptr_t)text)
uint32_t kernel_text_number8_81eaf2(PARAMETERS)
{NUMBER_BODY(kernel_text_run8_81e7b2);}
uint32_t kernel_text_number32_81ec98(PARAMETERS)
{NUMBER_BODY(kernel_text_run32_81e952);}
uint32_t kernel_number_digits_81ee3e(volatile uint32_t base,uint32_t number)
{
    uint32_t count=0;
    if(!number)return 1;
    while(number){number/=base;++count;}
    return count;
}
