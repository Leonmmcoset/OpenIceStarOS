/* Square-stamped lines/circles and masked circle pairs at 0x81d032..0x81d6c2.
 * Preserve wrapped ten-bit DDA geometry, eight-point order, side-specific
 * tests, live caller words and real calls through the existing service ABI.
 */
#include "raster_brush.h"
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define SLOT(id) DWORD(0x500000u+(id)*4u)
typedef uint32_t (*fill_fn)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
typedef uint32_t (*square_fn)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
typedef uint32_t (*point_fn)(uint32_t,uint32_t,uint32_t,uint32_t);
#define FILL ((fill_fn)(uintptr_t)0x837545u)
#define SQUARE ((square_fn)(uintptr_t)0x81d032u)
#define POINT ((point_fn)(uintptr_t)0x8374cfu)

__attribute__((optimize("no-omit-frame-pointer")))
uint32_t kernel_brush_square_81d032(uint32_t id,uint32_t x,uint32_t y,
    uint32_t color,uint32_t side)
{
    /* This frame also preserves the 32-bit fill's incidental raw EAX, which
     * points into the expired callee frame. Do not tail-call the forwarder. */
    return FILL(id,x,y,x+side-1u,y+side-1u,color);
}

uint32_t kernel_brush_line_81d070(volatile uint32_t id,volatile uint32_t x0,
    volatile uint32_t y0,volatile uint32_t x1,volatile uint32_t y1,
    volatile uint32_t color,volatile uint32_t side)
{
    uint32_t dx=x1-x0,dy=y1-y0,fx=x0<<10,fy=y0<<10,count;
    if((int32_t)dx<0)dx=0u-dx;
    if((int32_t)dy<0)dy=0u-dy;
    if((int32_t)dx>=(int32_t)dy){
        count=dx+1u;
        dx=(int32_t)x0>(int32_t)x1?0xfffffc00u:1024u;
        uint32_t numerator=(y1-y0+((int32_t)y0<=(int32_t)y1?1u:UINT32_MAX))<<10;
        dy=(uint32_t)((int32_t)numerator/(int32_t)count);
    }else{
        count=dy+1u;
        dy=(int32_t)y0>(int32_t)y1?0xfffffc00u:1024u;
        uint32_t numerator=(x1-x0+((int32_t)x0<=(int32_t)x1?1u:UINT32_MAX))<<10;
        dx=(uint32_t)((int32_t)numerator/(int32_t)count);
    }
    uint32_t i;
    for(i=0;(int32_t)i<(int32_t)count;++i){
        SQUARE(id,(uint32_t)((int32_t)fx>>10),(uint32_t)((int32_t)fy>>10),color,side);
        fx+=dx;fy+=dy;
    }
    return i;
}

/* The original zero-radius branch returns without touching EAX. A small
 * ABI entry keeps that behavior; the nonzero implementation remains C. */
__attribute__((naked,noinline))
uint32_t kernel_brush_circle_81d1fc(uint32_t id __attribute__((unused)),
    uint32_t x __attribute__((unused)),uint32_t y __attribute__((unused)),
    uint32_t radius __attribute__((unused)),uint32_t color __attribute__((unused)),
    uint32_t side __attribute__((unused)))
{
    __asm__("cmpl $0,16(%esp)\n\tje 1f\n\tjmp kernel_brush_circle_body\n1:\tret");
}
__attribute__((noinline,used))
uint32_t kernel_brush_circle_body(volatile uint32_t id,volatile uint32_t center_x,
    volatile uint32_t center_y,volatile uint32_t radius,
    volatile uint32_t color,volatile uint32_t side)
{
    uint32_t error=0u-(uint32_t)((int32_t)radius>>1),x=0,y=radius;
    while((int32_t)y>=(int32_t)x){
        uint32_t rx=x+center_x-1u,ry=y+center_x-1u;
        uint32_t lx=center_x-x,ly=center_x-y;
        uint32_t by=y+center_y-1u,bx=x+center_y-1u;
        uint32_t ty=center_y-y,tx=center_y-x;
        if((int32_t)DWORD(SLOT(id)+12u)>(int32_t)rx&&
           (int32_t)DWORD(SLOT(id)+16u)>(int32_t)by)SQUARE(id,rx,by,color,side);
        if((int32_t)DWORD(SLOT(id)+12u)>(int32_t)ry&&
           (int32_t)DWORD(SLOT(id)+16u)>(int32_t)bx)SQUARE(id,ry,bx,color,side);
        if((int32_t)lx>=0&&(int32_t)DWORD(SLOT(id)+16u)>(int32_t)by)SQUARE(id,lx,by,color,side);
        if((int32_t)ly>=0&&(int32_t)DWORD(SLOT(id)+16u)>(int32_t)bx)SQUARE(id,ly,bx,color,side);
        if((int32_t)lx>=0&&(int32_t)ty>=0)SQUARE(id,lx,ty,color,side);
        if((int32_t)ly>=0&&(int32_t)tx>=0)SQUARE(id,ly,tx,color,side);
        if((int32_t)DWORD(SLOT(id)+12u)>(int32_t)rx&&(int32_t)ty>=0)SQUARE(id,rx,ty,color,side);
        if((int32_t)DWORD(SLOT(id)+12u)>(int32_t)ry&&(int32_t)tx>=0)SQUARE(id,ry,tx,color,side);
        ++x;
        if((int32_t)error<0)error+=x;
        else{--y;error+=x-y;}
    }
    return y;
}

uint32_t kernel_circle_quadrants_81d4e6(volatile uint32_t id,
    volatile uint32_t center_x,volatile uint32_t center_y,volatile uint32_t radius,
    volatile uint32_t quadrants,volatile uint32_t color)
{
    uint32_t x=0,y=radius,error=3u-radius*2u;
    while((int32_t)x<=(int32_t)y){
        if(quadrants&1u){
            POINT(id,x+center_x,y+center_y,color);
            POINT(id,y+center_x,x+center_y,color);
        }
        if(quadrants&2u){
            POINT(id,center_x-x,y+center_y,color);
            POINT(id,center_x-y,x+center_y,color);
        }
        if(quadrants&4u){
            POINT(id,x+center_x,center_y-y,color);
            POINT(id,y+center_x,center_y-x,color);
        }
        if(quadrants&8u){
            POINT(id,center_x-x,center_y-y,color);
            POINT(id,center_x-y,center_y-x,color);
        }
        if((int32_t)error<0)error+=(x<<2)+6u;
        else{error+=((x-y)<<2)+10u;--y;}
        ++x;
    }
    return x;
}
#undef DWORD
#undef SLOT
#undef FILL
#undef SQUARE
#undef POINT
