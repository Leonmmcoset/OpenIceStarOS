/* Rounded/chamfered rectangle composition at 0x81d6c2..0x81db5a.
 * Coordinates and products wrap before signed comparisons. Calls remain
 * separate: drawing can overwrite the incoming geometry/color/ID words.
 */
#include "raster_rounded.h"
#include "service_forward.h"
#include "raster_brush.h"
#define QUADRANTS kernel_circle_quadrants_81d4e6
#define LINE kernel_service_01
#define FILL kernel_service_02
#define POINT kernel_service_00

/* Materialize one coordinate before loading the next live caller word.
 * This compiler-only barrier adds no machine instructions. */
static inline uint32_t coordinate(uint32_t value)
{
    __asm__ volatile("" : "+r"(value) : : "memory");
    return value;
}

uint32_t kernel_rounded_outline_81d6c2(volatile uint32_t id,volatile uint32_t x,
    volatile uint32_t y,volatile uint32_t width,volatile uint32_t height,
    volatile uint32_t radius,volatile uint32_t color)
{
    /* All four centers are captured before any drawing. */
    uint32_t ax=radius+x,ay=radius+y;
    uint32_t bx=width+x-radius-1u,by=radius+y;
    uint32_t cx=radius+x,cy=height+y-radius-1u;
    uint32_t dx=width+x-radius-1u,dy=height+y-radius-1u;
    QUADRANTS(id,ax,ay,radius,8,color);
    QUADRANTS(id,bx,by,radius,4,color);
    QUADRANTS(id,cx,cy,radius,2,color);
    QUADRANTS(id,dx,dy,radius,1,color);
    LINE(id,coordinate(radius+x),y,coordinate(width+x-radius),y,color);
    LINE(id,coordinate(radius+x),coordinate(height+y-1u),coordinate(width+x-radius),coordinate(height+y-1u),color);
    LINE(id,x,coordinate(radius+y),x,coordinate(height+y-radius),color);
    return LINE(id,coordinate(width+x-1u),coordinate(radius+y),coordinate(width+x-1u),coordinate(height+y-radius),color);
}

uint32_t kernel_rounded_fill_81d8ca(volatile uint32_t id,volatile uint32_t x,
    volatile uint32_t y,volatile uint32_t width,volatile uint32_t height,
    volatile uint32_t radius,volatile uint32_t color)
{
    uint32_t abi_frame;
    __asm__ volatile("" : "=m"(abi_frame));
    for(uint32_t i=0;(int32_t)i<(int32_t)(radius*2u);++i){
        for(uint32_t j=0;(int32_t)j<(int32_t)(radius*2u);++j){
            uint32_t dx=radius-i,dy=radius-j;
            if((int32_t)(dx*dx+dy*dy)<=(int32_t)(radius*radius)){
                POINT(id,coordinate(i+x),coordinate(j+y),color);
                POINT(id,coordinate(width+x-i-1u),coordinate(j+y),color);
                POINT(id,coordinate(i+x),coordinate(height+y-j-1u),color);
                POINT(id,coordinate(width+x-i-1u),coordinate(height+y-j-1u),color);
            }
        }
    }
    uint32_t far_y=coordinate(height+y-1u),far_x=coordinate(width+x-radius-1u),near_x=coordinate(radius+x);
    FILL(id,near_x,y,far_x,far_y,color);
    uint32_t bottom=coordinate(height+y-radius-1u),right=coordinate(width+x-1u),top=coordinate(radius+y);
    return FILL(id,x,top,right,bottom,color);
}

uint32_t kernel_chamfer_fill_81da68(volatile uint32_t id,volatile uint32_t x,
    volatile uint32_t y,volatile uint32_t width,volatile uint32_t height,
    volatile uint32_t radius,volatile uint32_t color)
{
    /* Reserve the original four-byte local slot, including after the loop.
     * The final fill must see its original outgoing argument-frame address. */
    uint32_t abi_frame;
    __asm__ volatile("" : "=m"(abi_frame));
    for(uint32_t i=0;(int32_t)i<(int32_t)radius;++i){
        uint32_t row=coordinate(i+y),left=coordinate(radius+x-i),right=coordinate(width+x-radius+i-1u);
        LINE(id,left,row,right,row,color);
        row=coordinate(height+y-i-1u);left=coordinate(radius+x-i);right=coordinate(width+x-radius+i-1u);
        LINE(id,left,row,right,row,color);
    }
    uint32_t bottom=coordinate(height+y-radius-1u),right=coordinate(width+x-1u),top=coordinate(radius+y);
    return FILL(id,x,top,right,bottom,color);
}
