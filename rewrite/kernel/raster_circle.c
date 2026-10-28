/* Eight-point circle outlines at 0x81c396 and 0x81c6f2. The original uses
 * center+offset-1 on positive sides, center-offset on negative sides, and
 * checks only the bound associated with each side. It can write outside
 * a surface; adding general clipping would change the recovered behavior.
 */
#include "raster_circle.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define SLOT(id) DWORD(0x500000u+(id)*4u)
#define WIDTH DWORD(SLOT(id)+12u)
#define HEIGHT DWORD(SLOT(id)+16u)

static __attribute__((always_inline)) inline void point8(volatile uint32_t *id,
    uint32_t x,uint32_t y,volatile uint32_t *color)
{
    uint32_t surface=SLOT(*id);
    uint32_t row=y*DWORD(SLOT(*id)+12u);
    uint32_t pixels=DWORD(surface),value=*color;
    BYTE(pixels+row+x)=(uint8_t)value;
}
static __attribute__((always_inline)) inline void point32(volatile uint32_t *id,
    uint32_t x,uint32_t y,volatile uint32_t *color)
{
    uint32_t offset=y*DWORD(SLOT(*id)+12u)+x;
    uint32_t surface=SLOT(*id),pixels=DWORD(surface),value=*color;
    DWORD(pixels+offset*4u)=value;
}
#define CIRCLE(bits,address) \
uint32_t kernel_raster_circle##bits##_##address(volatile uint32_t id, \
    volatile uint32_t center_x,volatile uint32_t center_y, \
    volatile uint32_t radius,volatile uint32_t color) \
{ \
    uint32_t error=0u-(uint32_t)((int32_t)radius>>1); \
    uint32_t x=0,y=radius; \
    while((int32_t)y>=(int32_t)x){ \
        uint32_t right_x=x+center_x-1u,right_y=y+center_x-1u; \
        uint32_t left_x=center_x-x,left_y=center_x-y; \
        uint32_t bottom_y=y+center_y-1u,bottom_x=x+center_y-1u; \
        uint32_t top_y=center_y-y,top_x=center_y-x; \
        if((int32_t)WIDTH>(int32_t)right_x&&(int32_t)HEIGHT>(int32_t)bottom_y) \
            point##bits(&id,right_x,bottom_y,&color); \
        if((int32_t)WIDTH>(int32_t)right_y&&(int32_t)HEIGHT>(int32_t)bottom_x) \
            point##bits(&id,right_y,bottom_x,&color); \
        if((int32_t)left_x>=0&&(int32_t)HEIGHT>(int32_t)bottom_y) \
            point##bits(&id,left_x,bottom_y,&color); \
        if((int32_t)left_y>=0&&(int32_t)HEIGHT>(int32_t)bottom_x) \
            point##bits(&id,left_y,bottom_x,&color); \
        if((int32_t)left_x>=0&&(int32_t)top_y>=0) \
            point##bits(&id,left_x,top_y,&color); \
        if((int32_t)left_y>=0&&(int32_t)top_x>=0) \
            point##bits(&id,left_y,top_x,&color); \
        if((int32_t)WIDTH>(int32_t)right_x&&(int32_t)top_y>=0) \
            point##bits(&id,right_x,top_y,&color); \
        if((int32_t)WIDTH>(int32_t)right_y&&(int32_t)top_x>=0) \
            point##bits(&id,right_y,top_x,&color); \
        ++x; \
        if((int32_t)error<0)error+=x; \
        else{--y;error+=x-y;} \
    } \
    return y; \
}
CIRCLE(8,81c396)
CIRCLE(32,81c6f2)

#undef BYTE
#undef DWORD
#undef SLOT
#undef WIDTH
#undef HEIGHT
#undef CIRCLE
