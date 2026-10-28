/* Original formatters, 0x81ee7e..0x81fd90. The return is the address of
 * the terminating format byte. X/Y and the format pointer are live incoming
 * words; varargs retain their initial stack cursor. Only widths 2..9 followed
 * by d/D/x/X are recognized. Unknown '%' forms print '%' without consuming
 * the next byte. CR resets X, while LF advances Y without resetting X.
 */
#include "text_format.h"
#include "text_glyph.h"
#include "text_wide.h"
#include "text_run.h"
#include "text_number.h"
uint32_t kernel_service_06(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
uint32_t kernel_service_07(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
uint32_t kernel_service_08(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
uint32_t kernel_service_09(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define NEXT BYTE(text+index+1u)
#define AFTER BYTE(text+index+2u)
#define PARAMETERS volatile uint32_t id,volatile uint32_t x,volatile uint32_t y, \
    volatile uint32_t foreground,volatile uint32_t background,volatile uint32_t text,...
#define FORMAT_BODY(glyph,wide,run,number) \
    uint32_t cursor=(uint32_t)(uintptr_t)&text+4u,index=0; \
    while(BYTE(text+index)){ \
        if(BYTE(text+index)=='%'){ \
            uint32_t width=0,length; \
            if((int8_t)NEXT>'1' && (int8_t)NEXT<='9' && \
               (AFTER=='d'||AFTER=='D'||AFTER=='x'||AFTER=='X')){ \
                width=(uint32_t)((int32_t)(int8_t)NEXT-48);++index; \
            } \
            if(NEXT=='c'||NEXT=='C'){ \
                length=glyph(id,x,y,foreground,background,(uint32_t)(int32_t)(int8_t)WORD(cursor)); \
                ++index;cursor+=4u; \
            }else if(NEXT=='d'||NEXT=='D'){ \
                uint32_t value=WORD(cursor); \
                if((int32_t)value<0){ \
                    value=0u-value;glyph(id,x,y,foreground,background,'-');x+=8u; \
                } \
                length=kernel_number_digits_81ee3e(10,value); \
                while((int32_t)width>(int32_t)length){ \
                    glyph(id,x,y,foreground,background,'0');x+=8u;--width; \
                } \
                length=number(id,x,y,foreground,background,10,value);++index;cursor+=4u; \
            }else if(NEXT=='u'||NEXT=='U'){ \
                length=kernel_number_digits_81ee3e(10,WORD(cursor)); \
                while((int32_t)width>(int32_t)length){ \
                    glyph(id,x,y,foreground,background,'0');x+=8u;--width; \
                } \
                length=number(id,x,y,foreground,background,10,WORD(cursor));++index;cursor+=4u; \
            }else if(NEXT=='x'||NEXT=='X'){ \
                length=kernel_number_digits_81ee3e(16,WORD(cursor)); \
                while((int32_t)width>(int32_t)length){ \
                    glyph(id,x,y,foreground,background,'0');x+=8u;--width; \
                } \
                length=number(id,x,y,foreground,background,16,WORD(cursor));++index;cursor+=4u; \
            }else if(NEXT=='s'||NEXT=='S'){ \
                length=run(id,x,y,foreground,background,WORD(cursor));++index;cursor+=4u; \
            }else{ \
                length=glyph(id,x,y,foreground,background,(uint32_t)(int32_t)(int8_t)BYTE(text+index)); \
            } \
            x+=length*8u; \
        }else if(BYTE(text+index)==13u){x=0;} \
        else if(BYTE(text+index)==10u){y+=16u;} \
        else if(BYTE(text+index)<=159u){ \
            glyph(id,x,y,foreground,background,(uint32_t)(int32_t)(int8_t)BYTE(text+index));x+=8u; \
        }else{wide(id,x,y,foreground,background,text+index);x+=16u;++index;} \
        ++index; \
    } \
    return text+index
uint32_t kernel_text_format8_81ee7e(PARAMETERS)
{FORMAT_BODY(kernel_text_glyph8_81db5a,kernel_text_wide8_81e106,kernel_text_run8_81e7b2,kernel_text_number8_81eaf2);}
uint32_t kernel_text_format32_81f384(PARAMETERS)
{FORMAT_BODY(kernel_text_glyph32_81de2a,kernel_text_wide32_81e45a,kernel_text_run32_81e952,kernel_text_number32_81ec98);}
uint32_t kernel_text_format_81f88a(PARAMETERS)
{FORMAT_BODY(kernel_service_06,kernel_service_07,kernel_service_08,kernel_service_09);}
