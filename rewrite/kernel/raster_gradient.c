/* Unclipped 32-bit rectangle with periodic color increments, 0x81bf50.
 * The counter continues between rows; color and increment restart each row.
 * X extrema also restart each row. Caller words/metadata may alias stores.
 */
#include "raster_gradient.h"
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define SLOT(id) DWORD(0x500000u+(id)*4u)
uint32_t kernel_raster_gradient32_81bf50(volatile uint32_t id,
    volatile uint32_t x0,volatile uint32_t y0,volatile uint32_t x1,
    volatile uint32_t y1,volatile uint32_t color,
    volatile uint32_t increment,volatile uint32_t period)
{
    uint32_t counter=0,ymin=y0;
    if((int32_t)ymin>(int32_t)y1)ymin=y1;
    uint32_t ymax=y0;
    if((int32_t)ymax<(int32_t)y1)ymax=y1;
    uint32_t y;
    for(y=ymin;(int32_t)y<=(int32_t)ymax;++y){
        uint32_t xmin=x0;
        if((int32_t)xmin>(int32_t)x1)xmin=x1;
        uint32_t xmax=x0;
        if((int32_t)xmax<(int32_t)x1)xmax=x1;
        uint32_t value=color,delta=increment;
        for(uint32_t x=xmin;(int32_t)x<=(int32_t)xmax;++x){
            uint32_t offset=DWORD(SLOT(id)+12u)*y+x;
            uint32_t surface=SLOT(id),pixels=DWORD(surface);
            DWORD(pixels+offset*4u)=value;
            ++counter;
            if(counter==period){value+=delta;counter=0;}
        }
    }
    return y;
}
#undef DWORD
#undef SLOT
