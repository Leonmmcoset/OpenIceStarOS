/* Window stacking, activation, focus loss and title-button hit selection.
 * Packed fields and unsigned surface-order comparisons follow the original. */
#include "window_focus.h"
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define OBJECT(id) D(0x6a0000u+4u*(id))
#define SURFACE(id) D(0x500000u+4u*(id))
#define OSURFACE(obj) SURFACE(D((obj)+12))
void kernel_focus_append_8179d6(uint32_t);
void kernel_focus_move_817b32(uint32_t,uint32_t);
void kernel_focus_front_817a3c(uint32_t);
void kernel_focus_caption_807540(uint32_t,uint32_t);
void kernel_focus_refresh_81905e(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);

static __attribute__((always_inline)) inline void place(uint32_t object,uint32_t id)
{
    if(D(SURFACE(D(0x1b0a4))+44)){
        if(D(object+0x11f)==4){
            D(0x1b138)=D(object+12);
            kernel_focus_move_817b32(D(object+12),D(SURFACE(D(0x1b0a4))+44)+1u);
        }else if(D(0x1b150)==id){
            kernel_focus_move_817b32(D(object+12),D(SURFACE(D(0x1b0a4))+44)+1u);
        }else if(D(OSURFACE(object)+44)>D(SURFACE(D(0x1b0a4))+44)){
            kernel_focus_move_817b32(D(object+12),D(SURFACE(D(0x1b0a4))+44));
        }else if(D(OSURFACE(object)+44)<D(SURFACE(D(0x1b0a4))+44)){
            kernel_focus_move_817b32(D(object+12),D(SURFACE(D(0x1b0a4))+44)-1u);
        }else if(D(SURFACE(1)+44))kernel_focus_front_817a3c(1);
    }else if(D(SURFACE(1)+44)){
        if(D(object+0x11f)==4)D(0x1b138)=D(object+12);
        if(D(OSURFACE(object)+44)>D(SURFACE(1)+44))
            kernel_focus_move_817b32(D(object+12),D(SURFACE(1)+44));
        else if(D(OSURFACE(object)+44)<D(SURFACE(1)+44))
            kernel_focus_move_817b32(D(object+12),D(SURFACE(1)+44)-1u);
    }
}

void kernel_window_raise_8084a0(volatile uint32_t id)
{
    if(!OBJECT(id))return;
    uint32_t object=OBJECT(id);
    if(D(object+0x127)==1)D(0x1b0ec)=1;
    if(!D(OSURFACE(object)+44))kernel_focus_append_8179d6(D(object+12));
    if(D(object+12)==2 || D(object+12)==3)return;
    place(object,id);
    kernel_focus_refresh_81905e(D(object+12),D(OSURFACE(object)+4),D(OSURFACE(object)+8),D(OSURFACE(object)+12),D(OSURFACE(object)+16));
    if(D(object+0x123)==1)D(object+0x123)=0;
}

void kernel_window_activate_8087e0(volatile uint32_t id)
{
    if(!OBJECT(id))return;
    uint32_t object=OBJECT(id);
    if(!D(object+0x10f)){
        D(object+0x10f)=1;
        if(D(object+0x127)==1)D(0x1b0ec)=1;
        D(0x1b0a0)=id;
        if(!D(OSURFACE(object)+44))kernel_focus_append_8179d6(D(object+12));
        if(D(object+12)==2 || D(object+12)==3)return;
        place(object,id);
        if((int32_t)D(object+0x113)>0 && (int32_t)D(object+0x113)<=3)
            kernel_focus_caption_807540(id,D(object+0x10f));
        if(D(object+0x55f))D(D(object+0x55f))=1;
    }
    kernel_focus_refresh_81905e(D(object+12),D(OSURFACE(object)+4),D(OSURFACE(object)+8),D(OSURFACE(object)+12),D(OSURFACE(object)+16));
}

void kernel_window_deactivate_808b78(volatile uint32_t id)
{
    if(!OBJECT(id))return;
    uint32_t object=OBJECT(id),parent=OBJECT(D(object+8)),current=OBJECT(D(0x1b0a0));
    if(!D(object+0x10f))return;
    D(object+0x10f)=0;
    if(D(current+8)!=id){
        if((int32_t)D(object+0x113)>0 && (int32_t)D(object+0x113)<=3)
            kernel_focus_caption_807540(id,D(object+0x10f));
        if((int32_t)D(object+8)>0 && D(current)!=D(object+8) &&
           (int32_t)D(parent+0x113)>0 && (int32_t)D(parent+0x113)<=3){
            kernel_focus_caption_807540(D(object+8),D(object+0x10f));
            kernel_focus_refresh_81905e(D(parent+12),D(OSURFACE(parent)+4),D(OSURFACE(parent)+8),D(OSURFACE(parent)+12),D(parent+0x157)+6u);
        }
    }
    if(D(object+0x5a7))((void(*)(uint32_t))(uintptr_t)D(object+0x5a7))(id);
    kernel_focus_refresh_81905e(D(object+12),D(OSURFACE(object)+4),D(OSURFACE(object)+8),D(OSURFACE(object)+12),D(OSURFACE(object)+16));
    if(D(object+0x55f))D(D(object+0x55f)+4)=1;
}

int32_t kernel_window_button_hit_808db8(uint32_t id,int32_t x,int32_t y)
{
    uint32_t object=OBJECT(id),theme=D(D(0x1b178)+D(object+0x113)*4u);
    int32_t mode=(int32_t)D(object+0x11b);
    uint32_t wide;
    if((int32_t)theme>0 && (int32_t)theme<=3)wide=0;
    else if(theme==4 || theme==5)wide=3;
    else return 0;
    /* Original rectangles exclude all four boundary lines. Arithmetic wraps
     * before signed comparison, including screen positions near INT32_MAX. */
#define BUTTON(left,right) ((int32_t)(D(OSURFACE(object)+4)+D(OSURFACE(object)+12)-(left)-wide)<x && \
    (int32_t)(D(OSURFACE(object)+4)+D(OSURFACE(object)+12)-(right)-wide)>x && \
    (int32_t)(D(OSURFACE(object)+8)+(wide?7u:5u))<y && \
    (int32_t)(D(OSURFACE(object)+8)+(wide?19u:15u))>y)
    if(mode==1)return BUTTON(24u,8u)?1:0;
    if(mode==2){if(BUTTON(24u,8u))return 1;return BUTTON(40u,24u)?4:0;}
    if(mode>2){
        if(BUTTON(24u,8u))return 1;
        if(BUTTON(59u,40u))return 2;
        if(BUTTON(40u,24u))return 3;
    }
    return 0;
#undef BUTTON
}
