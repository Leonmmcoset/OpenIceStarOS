/* CLOCK.X's reachable SDK boundaries. Keep real cdecl calls, separate
 * outgoing arguments, and a fresh shared-table read for every call. */
#include "legacy_clock_api.h"
#define SLOT(n, type) ((type)(uintptr_t)((volatile uint32_t *)(uintptr_t) \
                       *(volatile uint32_t *)0x1b0e4)[n])

uint32_t legacy_character(uint32_t s,uint32_t x,uint32_t y,uint32_t f,uint32_t b,int32_t c)
{
    typedef uint32_t (*fn)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,int32_t);
    return SLOT(6,fn)(s,x,y,f,b,c);
}
#define TEXT(name,slot) \
uint32_t name(uint32_t s,uint32_t x,uint32_t y,uint32_t f,uint32_t b,const char *p) \
{ typedef uint32_t (*fn)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,const char *); \
  return SLOT(slot,fn)(s,x,y,f,b,p); }
TEXT(legacy_chinese,7)
TEXT(legacy_string,8)
#define SEVEN(name,slot) \
uint32_t name(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e,uint32_t f,uint32_t g) \
{ typedef uint32_t (*fn)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t); \
  return SLOT(slot,fn)(a,b,c,d,e,f,g); }
SEVEN(legacy_number,9)
SEVEN(legacy_thick_line,239)
uint32_t legacy_thick_circle(uint32_t s,uint32_t x,uint32_t y,uint32_t r,uint32_t c,uint32_t w)
{
    typedef uint32_t (*fn)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
    return SLOT(240,fn)(s,x,y,r,c,w);
}
uint32_t legacy_digit_count(uint32_t base,uint32_t value)
{
    typedef uint32_t (*fn)(uint32_t,uint32_t);
    return SLOT(242,fn)(base,value);
}
#define TRIG(name,slot) \
double name(double angle) { typedef double (*fn)(double); return SLOT(slot,fn)(angle); }
TRIG(legacy_sine,244)
TRIG(legacy_cosine,245)
