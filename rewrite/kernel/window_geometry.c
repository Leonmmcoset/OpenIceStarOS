/* Original maximize, display-layout update and restore geometry contracts.
 * Surface backing and logical sizes remain distinct; callbacks stay live. */
#include "window_geometry.h"
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define OBJECT(id) D(0x6a0000u+4u*(id))
#define SURFACE(id) D(0x500000u+4u*(id))
#define OSURFACE(obj) SURFACE(D((obj)+12))
void kernel_geometry_free_815978(uint32_t);
uint32_t kernel_geometry_alloc_8155ca(uint32_t,uint32_t);
void kernel_geometry_draw_8045f0(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
void kernel_geometry_refresh_81905e(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);

static __attribute__((always_inline)) inline void backing(uint32_t object)
{
    if((int32_t)(D(OSURFACE(object)+20)*D(OSURFACE(object)+24)) <
       (int32_t)(D(SURFACE(0)+12)*D(SURFACE(0)+16))){
        kernel_geometry_free_815978(D(OSURFACE(object)));
        uint32_t surface=OSURFACE(object);
        uint32_t owner=D(object+4),pixels=D(SURFACE(0)+12)*D(SURFACE(0)+16);
        int32_t bytes=(int32_t)D(OSURFACE(object)+36)/8;
        D(surface)=kernel_geometry_alloc_8155ca(pixels*(uint32_t)bytes,owner);
    }
    D(OSURFACE(object)+20)=D(SURFACE(0)+12);
    D(OSURFACE(object)+24)=D(SURFACE(0)+16);
}

static __attribute__((always_inline)) inline void layout(uint32_t object)
{
    if(D(0x1b09c)==1){
        D(OSURFACE(object)+4)=0;D(OSURFACE(object)+8)=0;
        D(OSURFACE(object)+12)=D(OSURFACE(object)+20);
        D(OSURFACE(object)+16)=D(OSURFACE(object)+24)-D(SURFACE(D(0x1b0a4))+16);
    }else if(D(0x1b09c)==2){
        D(OSURFACE(object)+4)=D(SURFACE(D(0x1b0a4))+12);D(OSURFACE(object)+8)=0;
        D(OSURFACE(object)+12)=D(OSURFACE(object)+20)-D(SURFACE(D(0x1b0a4))+12);
        D(OSURFACE(object)+16)=D(OSURFACE(object)+24);
    }else if(D(0x1b09c)==3){
        D(OSURFACE(object)+4)=0;D(OSURFACE(object)+8)=0;
        D(OSURFACE(object)+12)=D(OSURFACE(object)+20)-D(SURFACE(D(0x1b0a4))+12);
        D(OSURFACE(object)+16)=D(OSURFACE(object)+24);
    }else if(D(0x1b09c)==4){
        D(OSURFACE(object)+4)=0;D(OSURFACE(object)+8)=D(SURFACE(D(0x1b0a4))+16);
        D(OSURFACE(object)+12)=D(OSURFACE(object)+20);
        D(OSURFACE(object)+16)=D(OSURFACE(object)+24)-D(SURFACE(D(0x1b0a4))+16);
    }else if(D(0x1b09c)==0 || D(0x1b09c)==5){
        D(OSURFACE(object)+4)=0;D(OSURFACE(object)+8)=0;
        D(OSURFACE(object)+12)=D(OSURFACE(object)+20);
        D(OSURFACE(object)+16)=D(OSURFACE(object)+24);
    }
}
static __attribute__((always_inline)) inline void client(uint32_t object)
{
    D(object+0x143)=D(OSURFACE(object)+12)-D(object+0x13b)-D(object+0x13b);
    D(object+0x147)=D(OSURFACE(object)+16)-D(object+0x13f)-D(object+0x13b);
    D(object+0x153)=D(OSURFACE(object)+12)-6u;
}
static __attribute__((always_inline)) inline void draw(uint32_t object,uint32_t initialize)
{
    kernel_geometry_draw_8045f0(D(object+12),object+16,D(object+0x10f),D(object+0x113),D(object+0x117),D(object+0x11b),initialize);
}
static __attribute__((always_inline)) inline void callback(uint32_t object,uint32_t id)
{
    if(D(object+0x573))((void(*)(uint32_t))(uintptr_t)D(object+0x573))(id);
}

void kernel_window_maximize_80ed56(volatile uint32_t id)
{
    uint32_t object=OBJECT(id);
    if((int32_t)D(object+0x113)<=0 || D(object+0x11b)!=3 || D(object+0x11f))return;
    backing(object);
    D(object+0x12b)=D(OSURFACE(object)+4);D(object+0x12f)=D(OSURFACE(object)+8);
    D(object+0x133)=D(OSURFACE(object)+12);D(object+0x137)=D(OSURFACE(object)+16);
    layout(object);client(object);
    D(object+0x11f)=1;D(object+0x11b)=4;
    draw(object,1);callback(object,id);draw(object,0);
    kernel_geometry_refresh_81905e(D(object+12),D(OSURFACE(object)+4),D(OSURFACE(object)+8),D(OSURFACE(object)+12),D(OSURFACE(object)+16));
}

void kernel_window_layout_all_80f4be(void)
{
    for(uint32_t id=0;(int32_t)id<(int32_t)D(0x1b098);++id){
        if((int32_t)OBJECT(id)<=0)continue;
        uint32_t object=OBJECT(id);
        if(D(object+0x11f)==1){
            backing(object);layout(object);client(object);draw(object,1);callback(object,id);
        }
        if(!D(object+0x11f))callback(object,id);
    }
}

void kernel_window_unmaximize_80fbfc(volatile uint32_t id)
{
    uint32_t object=OBJECT(id);
    if((int32_t)D(object+0x113)<=0 || D(object+0x11b)!=4 || D(object+0x11f)!=1)return;
    D(OSURFACE(object)+4)=D(object+0x12b);D(OSURFACE(object)+8)=D(object+0x12f);
    D(OSURFACE(object)+12)=D(object+0x133);D(OSURFACE(object)+16)=D(object+0x137);
    client(object);D(object+0x11f)=0;D(object+0x11b)=3;
    draw(object,1);callback(object,id);draw(object,0);
    if(D(0x1b09c)==1)
        kernel_geometry_refresh_81905e(2,0,0,D(SURFACE(2)+20),D(SURFACE(2)+24)-D(SURFACE(D(0x1b0a4))+16));
    else if(D(0x1b09c)==2)
        kernel_geometry_refresh_81905e(2,D(SURFACE(D(0x1b0a4))+12),0,D(SURFACE(2)+20)-D(SURFACE(D(0x1b0a4))+12),D(SURFACE(2)+24));
    else if(D(0x1b09c)==3)
        kernel_geometry_refresh_81905e(2,0,0,D(SURFACE(2)+20)-D(SURFACE(D(0x1b0a4))+12),D(SURFACE(2)+24));
    else if(D(0x1b09c)==4)
        kernel_geometry_refresh_81905e(2,0,D(SURFACE(D(0x1b0a4))+16),D(SURFACE(2)+20),D(SURFACE(2)+24)-D(SURFACE(D(0x1b0a4))+16));
    else if(D(0x1b09c)==0 || D(0x1b09c)==5)
        kernel_geometry_refresh_81905e(2,0,0,D(SURFACE(2)+20),D(SURFACE(2)+24));
}
