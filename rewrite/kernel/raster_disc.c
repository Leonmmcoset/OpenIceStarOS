/* Filled circles at 0x81ca82..0x81d032. Keep the original decimal root
 * refinement, binary64 stores, live caller words, repeated clamps and
 * asymmetric Y clipping. In particular, the right-edge clamp can prevent
 * termination. No geometry or iteration bound is added here.
 */
#include "raster_disc.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define SLOT(id) DWORD(0x500000u+(id)*4u)
#define WIDTH DWORD(SLOT(id)+12u)
#define HEIGHT DWORD(SLOT(id)+16u)
#define INLINE static __attribute__((always_inline)) inline

INLINE uint32_t decimal_root(uint32_t radicand)
{
    uint32_t scale=1;
    while((int32_t)radicand/(int32_t)scale>0)scale*=10u;
    volatile double estimate=(int32_t)scale/10;
    for(int32_t digit=16;digit>0;--digit){
        volatile double step=(int32_t)scale;
        for(int32_t position=16;position>=digit;--position)
            step=step*(*(volatile double *)(uintptr_t)0x83cf43u);
        while(__builtin_isgreater((double)(int32_t)radicand,estimate*estimate))
            estimate=estimate+step;
        estimate=estimate-step;
    }
    return (uint32_t)(int32_t)estimate;
}
INLINE void point8(volatile uint32_t *id,uint32_t x,
                   volatile uint32_t *y,volatile uint32_t *color)
{
    uint32_t surface=SLOT(*id);
    uint32_t width=DWORD(SLOT(*id)+12u),row=width * *y;
    uint32_t pixels=DWORD(surface),value=*color;
    BYTE(pixels+row+x)=(uint8_t)value;
}
INLINE void point32(volatile uint32_t *id,uint32_t x,
                    volatile uint32_t *y,volatile uint32_t *color)
{
    uint32_t width=DWORD(SLOT(*id)+12u),offset=width * *y+x;
    uint32_t surface=SLOT(*id),pixels=DWORD(surface),value=*color;
    DWORD(pixels+offset*4u)=value;
}
#define CLAMP_X() do{ \
    if((int32_t)x<0)x=0; \
    if((int32_t)WIDTH<=(int32_t)x)x=WIDTH-1u; \
}while(0)
#define DISC(bits,address) \
uint32_t kernel_raster_disc##bits##_##address(volatile uint32_t id, \
    volatile uint32_t center_x,volatile uint32_t center_y, \
    volatile uint32_t radius,volatile uint32_t color) \
{ \
    uint32_t x=center_x-radius; \
    while((int32_t)(radius+center_x)>=(int32_t)x){ \
        CLAMP_X(); \
        point##bits(&id,x,&center_y,&color); \
        ++x; \
    } \
    uint32_t offset; \
    for(offset=1;(int32_t)offset<=(int32_t)radius;++offset){ \
        uint32_t first_radius=radius,second_radius=radius; \
        uint32_t half_width=decimal_root(first_radius*second_radius-offset*offset); \
        volatile uint32_t bottom=offset+center_y,top=center_y-offset; \
        if((int32_t)HEIGHT<=(int32_t)bottom)bottom=HEIGHT-1u; \
        if((int32_t)top<0)top=0; \
        x=center_x-half_width; \
        while((int32_t)(half_width+center_x)>=(int32_t)x){ \
            CLAMP_X(); \
            point##bits(&id,x,&bottom,&color); \
            point##bits(&id,x,&top,&color); \
            ++x; \
        } \
    } \
    return offset; \
}
DISC(8,81ca82)
DISC(32,81cd50)

#undef BYTE
#undef DWORD
#undef SLOT
#undef WIDTH
#undef HEIGHT
#undef INLINE
#undef CLAMP_X
#undef DISC
