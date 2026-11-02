/* Window construction and close, preserving the packed object ABI, callback
 * order, failure-side CLI behavior and post-free completion-word access. */
#include "window_lifecycle.h"
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define OBJECT(id) D(0x6a0000u+4u*(id))
#define SURFACE(id) D(0x500000u+4u*(id))
#define OSURFACE(obj) SURFACE(D((obj)+12))
uint32_t kernel_window_flags_81a24a(void);
void kernel_window_cli_81a242(void);
void kernel_window_restore_81a24d(uint32_t);
uint32_t kernel_window_alloc_8155ca(uint32_t,uint32_t);
void kernel_window_free_815978(uint32_t);
int32_t kernel_window_slot_807990(void);
int32_t kernel_window_surface_817626(uint32_t,uint32_t,uint32_t,uint32_t);
void kernel_window_copy_82a32b(uint32_t,uint32_t);
void kernel_window_redraw_8045f0(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
void kernel_window_activate_8087e0(uint32_t);
void kernel_window_remove_surface_8178f0(uint32_t);
void kernel_window_refresh_81905e(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
void kernel_window_unregister_8105dc(uint32_t);
void kernel_window_delete_807f72(uint32_t);
uint32_t kernel_window_find_hit_8078f2(uint32_t,uint32_t);

int32_t kernel_window_create_807a0a(volatile uint32_t x,volatile uint32_t y,
    volatile uint32_t width,volatile uint32_t height,volatile int32_t style,
    volatile uint32_t parameter,volatile uint32_t mode,volatile uint32_t title)
{
    uint32_t styles=D(0x1b178),flags=kernel_window_flags_81a24a();kernel_window_cli_81a242();
    uint32_t object=kernel_window_alloc_8155ca(0x5bb,D(0x1b0d8));
    int32_t id=kernel_window_slot_807990();
    if(!object)return -1;
    if(id<0){kernel_window_free_815978(object);return -1;}
    D(object)=id;D(object+4)=D(0x1b0d8);D(object+8)=0;
    if(mode==3){
        D(object+12)=kernel_window_surface_817626(x,y,D(SURFACE(0)+12),D(SURFACE(0)+16));
        D(OSURFACE(object)+12)=width;D(OSURFACE(object)+16)=height;
        D(OSURFACE(object)+48)=D(object);
    }else{
        D(object+12)=kernel_window_surface_817626(x,y,width,height);
        D(OSURFACE(object)+48)=D(object);
    }
    uint32_t theme=(style>=0 && style<=3)?D(styles+(uint32_t)style*4u):0;
    kernel_window_copy_82a32b(object+16,title);
    D(object+0x10f)=0;D(object+0x113)=style;D(object+0x11b)=mode;
    D(object+0x127)=1;D(object+0x11f)=0;D(object+0x123)=1;
    D(object+0x117)=parameter;
    D(object+0x12b)=x;D(object+0x12f)=y;D(object+0x133)=width;D(object+0x137)=height;
    D(object+0x15b)=0;
    if(theme==1 || theme==2 || theme==3){
        D(object+0x13b)=6;D(object+0x13f)=28;D(object+0x143)=width-10u;D(object+0x147)=height-34u;
        D(object+0x14b)=3;D(object+0x14f)=3;D(object+0x153)=width-6u;D(object+0x157)=20;
    }else if(theme==4 || theme==5){
        D(object+0x13b)=8;D(object+0x13f)=32;D(object+0x143)=width-14u;D(object+0x147)=height-40u;
        D(object+0x14b)=3;D(object+0x14f)=3;D(object+0x153)=width-6u;D(object+0x157)=24;
    }else{
        D(object+0x13b)=0;D(object+0x13f)=0;D(object+0x143)=width;D(object+0x147)=height;
        D(object+0x14b)=0;D(object+0x14f)=0;D(object+0x153)=0;D(object+0x157)=0;
    }
    D(object+0x55f)=0;D(object+0x563)=0;D(object+0x567)=0;D(object+0x56b)=0;
    D(object+0x56f)=0;D(object+0x573)=0;D(object+0x577)=0;D(object+0x57b)=0;
    D(object+0x583)=0;D(object+0x57f)=0;D(object+0x587)=0;D(object+0x58b)=0;
    D(object+0x58f)=0;D(object+0x59b)=0;D(object+0x593)=0;D(object+0x597)=0;
    D(object+0x5b7)=0;D(object+0x5a7)=0;
    if((int32_t)D(object+12)<0){kernel_window_free_815978(object);return -1;}
    D(OSURFACE(object)+48)=id;
    kernel_window_redraw_8045f0(D(object+12),object+16,D(object+0x10f),D(object+0x113),D(object+0x117),mode,1);
    OBJECT((uint32_t)id)=object;
    if((int32_t)D(0x1b098)>id)D(0x1b094)=D(0x1b094)-1u;else D(0x1b098)=D(0x1b098)+1u;
    kernel_window_restore_81a24d(flags);return id;
}

void kernel_window_close_81021c(volatile uint32_t id)
{
    uint32_t flags=kernel_window_flags_81a24a();kernel_window_cli_81a242();
    uint32_t object=OBJECT(id);
    if(D(object+0x5b7))((void(*)(uint32_t))(uintptr_t)D(object+0x5b7))(id);
    if(D(object+12)==D(0x1b138))D(0x1b138)=0;
    if(D(OSURFACE(object)+44)){
        if(D(object+0x10f)==1){
            uint32_t previous=D(0x510000+D(OSURFACE(object)+44)*4u-4u);
            kernel_window_activate_8087e0(D(SURFACE(previous)+48));D(object+0x10f)=0;
        }
        if(!D(object+0x123)){kernel_window_remove_surface_8178f0(D(object+12));D(object+0x123)=1;}
        kernel_window_refresh_81905e(2,D(OSURFACE(object)+4),D(OSURFACE(object)+8),D(OSURFACE(object)+12),D(OSURFACE(object)+16));
    }
    if(id && D(object+0x127)==1){kernel_window_unregister_8105dc(id);D(0x1b0ec)=1;}
    kernel_window_delete_807f72(id);
    D(0x1b16c)=kernel_window_find_hit_8078f2(D(SURFACE(1)+4),D(SURFACE(1)+8));
    if(D(object+0x55f))D(D(object+0x55f)+8)=1;
    kernel_window_restore_81a24d(flags);
}
