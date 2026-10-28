/* Mixed single/two-byte text runs at 0x81e7b2..0x81eaf2. The byte at the
 * next index is read before the <=254 limit. Invalid glyphs still consume
 * one/two bytes. Vertical runs use rotated single-byte glyphs but ordinary
 * two-byte glyphs, preserving the original mixed orientation.
 */
#include "text_run.h"
#include "text_glyph.h"
#include "text_wide.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define RUN_BODY(ascii,wide,vertical) \
    uint32_t i=0; \
    while(BYTE(text+i)!=0 && (int32_t)i<=254){ \
        if(BYTE(text+i)<=159u){ \
            ascii(id,vertical?x:x+i*8u,vertical?y-i*8u:y,foreground,background, \
                (uint32_t)(int32_t)(int8_t)BYTE(text+i)); \
            ++i; \
        }else{ \
            wide(id,vertical?x:x+i*8u,vertical?y-i*8u:y,foreground,background,text+i); \
            i+=2u; \
        } \
    } \
    return i
#define PARAMETERS volatile uint32_t id,volatile uint32_t x,volatile uint32_t y, \
    volatile uint32_t foreground,volatile uint32_t background,volatile uint32_t text
uint32_t kernel_text_run8_81e7b2(PARAMETERS)
{RUN_BODY(kernel_text_glyph8_81db5a,kernel_text_wide8_81e106,0);}
uint32_t kernel_text_vertical8_81e87c(PARAMETERS)
{RUN_BODY(kernel_text_rotated8_81dcc0,kernel_text_wide8_81e106,1);}
uint32_t kernel_text_run32_81e952(PARAMETERS)
{RUN_BODY(kernel_text_glyph32_81de2a,kernel_text_wide32_81e45a,0);}
uint32_t kernel_text_vertical32_81ea1c(PARAMETERS)
{RUN_BODY(kernel_text_rotated32_81df96,kernel_text_wide32_81e45a,1);}
