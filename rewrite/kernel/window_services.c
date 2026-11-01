/* Window/UI services recovered from original fixed-address entries.
 * Numeric fields preserve the packed, unaligned object ABI. No validation,
 * clipping or callback normalization is added to the original contracts. */
#include "window_services.h"
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define B(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define OBJECT(id) D(0x6a0000u+4u*(id))
#define SURFACE(id) D(0x500000u+4u*(id))
#define OSURFACE(obj) SURFACE(D((obj)+12))
void kernel_ui_copy_82a32b(uint32_t,uint32_t);
void kernel_ui_task_814688(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
void kernel_ui_fill_837545(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
void kernel_ui_outline_837589(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
void kernel_ui_caption_a_806742(uint32_t,uint32_t,uint32_t,uint32_t);
void kernel_ui_caption_b_806228(uint32_t,uint32_t,uint32_t,uint32_t);
void kernel_ui_caption_c_805e84(uint32_t,uint32_t,uint32_t,uint32_t);
void kernel_ui_surface_delete_81783e(uint32_t);
void kernel_ui_free_815978(uint32_t);
uint32_t kernel_ui_icon_path_827798(uint32_t);
uint32_t kernel_ui_icon_relative_826f38(uint32_t);
void kernel_ui_surface_add_8179d6(uint32_t);
void kernel_ui_surface_order_817b32(uint32_t,uint32_t);
void kernel_ui_refresh_81905e(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
void kernel_ui_surface_remove_8178f0(uint32_t);
void kernel_ui_activate_8087e0(uint32_t);
void kernel_ui_deactivate_808b78(uint32_t);
void kernel_ui_registry_add_81054e(uint32_t);
void kernel_ui_registry_remove_8105dc(uint32_t);
void kernel_ui_move_refresh_8191d8(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
void kernel_ui_redraw_8045f0(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
void kernel_ui_close_81021c(uint32_t);
void kernel_ui_image_release_8125dc(uint32_t);
void kernel_ui_mailbox_put_802e64(volatile uint32_t text)
{
    uint32_t mailbox=D(0x1b134),busy;
    do{busy=0;for(uint32_t i=0;i<=255;++i)if(B(mailbox+i)){busy=1;break;}}while(busy);
    kernel_ui_copy_82a32b(mailbox,text);
}
void kernel_ui_mailbox_get_802ec6(volatile uint32_t flag,volatile uint32_t target)
{uint32_t mailbox=D(0x1b134);kernel_ui_copy_82a32b(target,mailbox);D(flag)=1;}
void kernel_ui_launch_path_802ef2(volatile uint32_t text)
{kernel_ui_mailbox_put_802e64(text);kernel_ui_task_814688(0x80252a,0x95000,0x40,0x38,8,0,0x83ccb8);}
void kernel_ui_launch_relative_802f42(volatile uint32_t text)
{kernel_ui_mailbox_put_802e64(text);kernel_ui_task_814688(0x802c08,0x95000,0x40,0x38,8,0,0x83ccb8);}
void kernel_ui_frame_807302(volatile uint32_t id,volatile uint32_t x,volatile uint32_t y,volatile uint32_t width,volatile uint32_t height,volatile uint32_t color)
{
    if((int32_t)width<0 || (int32_t)height<0)return;
    uint32_t right=x+width,bottom=y+height,theme=D(0x1b0e0);
    kernel_ui_fill_837545(id,x-2,y-3,right+1,y-3,D(theme+60));
    kernel_ui_fill_837545(id,x-3,y-3,x-3,bottom+1,D(theme+60));
    kernel_ui_fill_837545(id,x-3,bottom+2,right+1,bottom+2,D(theme+28));
    kernel_ui_fill_837545(id,right+2,y-3,right+2,bottom+2,D(theme+28));
    kernel_ui_fill_837545(id,x-1,y-2,right,y-2,D(theme));
    kernel_ui_fill_837545(id,x-2,y-2,x-2,bottom,D(theme));
    kernel_ui_fill_837545(id,x-2,bottom+1,right,bottom+1,D(theme+32));
    kernel_ui_fill_837545(id,right+1,y-2,right+1,bottom+1,D(theme+32));
    kernel_ui_fill_837545(id,x-1,y-1,right,bottom,color);
}
void kernel_ui_caption_807540(volatile uint32_t id,volatile uint32_t active)
{
    uint32_t object=OBJECT(id),theme=D(0x1b178),style=D(theme+D(object+0x113)*4);
    if(style==4 || style==5)kernel_ui_caption_a_806742(D(object+12),object+16,active,D(object+0x11b));
    else if(style==3)kernel_ui_caption_b_806228(D(object+12),object+16,active,D(object+0x11b));
    else if(style==1 || style==2)kernel_ui_caption_c_805e84(D(object+12),object+16,active,D(object+0x11b));
}
uint32_t kernel_ui_title_hit_80762e(volatile uint32_t id,volatile uint32_t x,volatile uint32_t y)
{
    return (int32_t)(D(SURFACE(id)+4)+3)<(int32_t)x &&
        (int32_t)(D(SURFACE(id)+4)+D(SURFACE(id)+12)-22)>(int32_t)x &&
        (int32_t)(D(SURFACE(id)+8)+3)<(int32_t)y &&
        (int32_t)(D(SURFACE(id)+8)+20)>(int32_t)y;
}
uint32_t kernel_ui_client_hit_8076d0(volatile uint32_t id,volatile uint32_t x,volatile uint32_t y)
{
    uint32_t object=OBJECT(id),pixels=D(OSURFACE(object));
    uint32_t row=(y-D(OSURFACE(object)+8))*D(OSURFACE(object)+12),col=x-D(OSURFACE(object)+4);
    if(B(pixels+row+col)==D(OSURFACE(object)+40))return 0;
    return (int32_t)(D(object+0x14b)+D(OSURFACE(object)+4))<(int32_t)x &&
        (int32_t)(D(OSURFACE(object)+4)+D(object+0x14b)+D(object+0x153))>(int32_t)x &&
        (int32_t)(D(object+0x14f)+D(OSURFACE(object)+8))<(int32_t)y &&
        (int32_t)(D(OSURFACE(object)+8)+D(object+0x14f)+D(object+0x157))>(int32_t)y;
}
uint32_t kernel_ui_surface_hit_80784a(volatile uint32_t id,volatile uint32_t x,volatile uint32_t y)
{
    return (int32_t)D(SURFACE(id)+4)<(int32_t)x &&
        (int32_t)(D(SURFACE(id)+4)+D(SURFACE(id)+12))>(int32_t)x &&
        (int32_t)D(SURFACE(id)+8)<(int32_t)y &&
        (int32_t)(D(SURFACE(id)+8)+D(SURFACE(id)+16))>(int32_t)y;
}
uint32_t kernel_ui_find_hit_8078f2(volatile uint32_t x,volatile uint32_t y)
{
    uint32_t n=D(0x510000);if(D(SURFACE(1)+44))--n;
    for(;(int32_t)n>0;--n)if(kernel_ui_surface_hit_80784a(D(0x510000+n*4),x,y))return D(SURFACE(D(0x510000+n*4))+48);
    return 0xffffffff;
}
uint32_t kernel_ui_object_slot_807990(void)
{
    if((int32_t)D(0x1b094)>0)for(uint32_t i=0;(int32_t)i<=(int32_t)D(0x1b098);++i)if(!OBJECT(i))return i;
    return (int32_t)D(0x1b098)<(int32_t)D(0x1b090)?D(0x1b098):0xffffffff;
}
void kernel_ui_delete_807f72(volatile uint32_t id)
{
    uint32_t object=OBJECT(id);kernel_ui_surface_delete_81783e(D(object+12));
    if(D(object+0x56b) && D(object+0x567)==1)kernel_ui_free_815978(D(object+0x56b));
    kernel_ui_free_815978(OBJECT(id));OBJECT(id)=0;D(0x1b094)=D(0x1b094)+1;
}
void kernel_ui_icon_file_807ffe(volatile uint32_t id,volatile uint32_t path,volatile uint32_t flags)
{
    uint32_t object=OBJECT(id);
    uint32_t icon=(B(path+1)==':' && B(path+2)=='\\')?kernel_ui_icon_path_827798(path):kernel_ui_icon_relative_826f38(path);
    D(object+0x56b)=icon;
    if(D(object+0x56b)){D(object+0x567)=1;D(object+0x56f)=flags;}
}
void kernel_ui_icon_pointer_808090(volatile uint32_t id,volatile uint32_t icon,volatile uint32_t flags)
{
    uint32_t object=OBJECT(id);D(object+0x56b)=icon;
    if(D(object+0x56b)){D(object+0x567)=0;D(object+0x56f)=flags;}
}
#define REFRESH(obj,target) kernel_ui_refresh_81905e(target,D(OSURFACE(obj)+4),D(OSURFACE(obj)+8),D(OSURFACE(obj)+12),D(OSURFACE(obj)+16))
void kernel_ui_restore_8080ea(volatile uint32_t id)
{
    uint32_t object=OBJECT(id);if(D(object+0x123)!=1)return;
    D(object+0x123)=0;
    if(!D(OSURFACE(object)+44)){kernel_ui_surface_add_8179d6(D(object+12));kernel_ui_surface_order_817b32(D(object+12),D(SURFACE(1)+44));}
    REFRESH(object,D(object+12));
}
void kernel_ui_hide_8081ea(volatile uint32_t id)
{
    uint32_t object=OBJECT(id);if(!D(OSURFACE(object)+44))return;
    kernel_ui_surface_remove_8178f0(D(object+12));D(object+0x123)=1;REFRESH(object,2);
}
void kernel_ui_show_8082c6(volatile uint32_t id)
{
    uint32_t old=D(0x1b0a0);if(!OBJECT(id))return;
    uint32_t object=OBJECT(id);
    if(!D(object+0x10f)){kernel_ui_activate_8087e0(id);kernel_ui_deactivate_808b78(old);}else kernel_ui_activate_8087e0(id);
    if(D(object+0x123)==1)D(object+0x123)=0;
    if(id && D(object+0x127)==1){kernel_ui_registry_add_81054e(id);D(0x1b0ec)=1;}
}
void kernel_ui_remove_808378(volatile uint32_t id)
{
    if(!OBJECT(id))return;
    uint32_t object=OBJECT(id);
    if(D(OSURFACE(object)+44)){kernel_ui_surface_remove_8178f0(D(object+12));D(object+0x10f)=0;D(object+0x123)=1;REFRESH(object,2);}
    if(id && D(object+0x127)==1){kernel_ui_registry_remove_8105dc(id);D(0x1b0ec)=1;}
}
void kernel_ui_event4_809e24(volatile uint32_t id,volatile uint32_t a,volatile uint32_t b,volatile uint32_t c)
{
    uint32_t object=OBJECT(id);if(D(object+0x583))((void(*)(uint32_t,uint32_t,uint32_t,uint32_t))(uintptr_t)D(object+0x583))(id,a,b,c);
}
void kernel_ui_event3_80ac4e(volatile uint32_t id,volatile uint32_t a,volatile uint32_t b)
{
    uint32_t object=OBJECT(id);if(D(object+0x58b))((void(*)(uint32_t,uint32_t,uint32_t))(uintptr_t)D(object+0x58b))(id,a,b);
}
void kernel_ui_move_80ac9c(volatile uint32_t id,volatile uint32_t dx,volatile uint32_t dy)
{
    if(D(0x1b120))return;
    uint32_t object=OBJECT(id),x=D(OSURFACE(object)+4),y=D(OSURFACE(object)+8);
    D(OSURFACE(object)+4)=dx+D(OSURFACE(object)+4);D(OSURFACE(object)+8)=dy+D(OSURFACE(object)+8);
    D(object+0x12b)=D(OSURFACE(object)+4);D(object+0x12f)=D(OSURFACE(object)+8);
    if(D(object+0x58f))((void(*)(uint32_t,uint32_t,uint32_t))(uintptr_t)D(object+0x58f))(id,dx,dy);
    kernel_ui_move_refresh_8191d8(D(object+12),x,y,D(OSURFACE(object)+12),D(OSURFACE(object)+16),D(OSURFACE(object)+4),D(OSURFACE(object)+8));
}
void kernel_ui_event3b_80ecba(volatile uint32_t id,volatile uint32_t a,volatile uint32_t b)
{
    uint32_t object=OBJECT(id);if(D(object+0x593))((void(*)(uint32_t,uint32_t,uint32_t))(uintptr_t)D(object+0x593))(id,a,b);
}
void kernel_ui_event3c_80ed08(volatile uint32_t id,volatile uint32_t a,volatile uint32_t b)
{
    uint32_t object=OBJECT(id);if(D(object+0x597))((void(*)(uint32_t,uint32_t,uint32_t))(uintptr_t)D(object+0x597))(id,a,b);
}
void kernel_ui_redraw_all_80fb12(void)
{
    for(uint32_t i=0;(int32_t)i<(int32_t)D(0x1b098);++i)if((int32_t)OBJECT(i)>0){
        uint32_t object=OBJECT(i);
        if((int32_t)D(object+0x113)>0)kernel_ui_redraw_8045f0(D(object+12),object+16,D(object+0x10f),D(object+0x113),D(object+0x117),D(object+0x11b),0);
        if(D(object+0x573))((void(*)(uint32_t))(uintptr_t)D(object+0x573))(i);
    }
}
void kernel_ui_minimize_81002e(volatile uint32_t id)
{
    uint32_t object=OBJECT(id);if(!D(OSURFACE(object)+44))return;
    if(D(object+0x10f)==1){kernel_ui_activate_8087e0(D(SURFACE(D(0x510000+D(OSURFACE(object)+44)*4-4))+48));D(object+0x10f)=0;}
    if(!D(object+0x123)){kernel_ui_surface_remove_8178f0(D(object+12));D(object+0x123)=1;}
    REFRESH(object,2);
}
void kernel_ui_close_api_810168(uint32_t id)
{kernel_ui_close_81021c(id);}
void kernel_ui_dialog_click_811c46(volatile uint32_t id,volatile uint32_t x,volatile uint32_t y)
{
    uint32_t object=OBJECT(id);
#define HIT(a,b) ((int32_t)(D(OSURFACE(object)+4)+(a))<(int32_t)x && (int32_t)(D(OSURFACE(object)+4)+(b))>(int32_t)x && (int32_t)(D(OSURFACE(object)+8)+100)<(int32_t)y && (int32_t)(D(OSURFACE(object)+8)+127)>(int32_t)y)
    if(D(object+0x15b)==1){if(HIT(85,144))kernel_ui_close_81021c(id);}
    else if(D(object+0x15b)==2){if(HIT(40,99)){D(object+0x563)=1;kernel_ui_close_81021c(id);}else if(HIT(130,189))kernel_ui_close_81021c(id);}
#undef HIT
}
void kernel_ui_double_border_811e4c(volatile uint32_t id,volatile uint32_t x0,volatile uint32_t y0,volatile uint32_t x1,volatile uint32_t y1)
{
    uint32_t theme=D(0x1b0e0);kernel_ui_outline_837589(id,x0,y0,x1,y1,D(theme+28));kernel_ui_outline_837589(id,x0+1,y0+1,x1-1,y1-1,D(theme+60));
}
uint32_t kernel_ui_image_slot_81215a(void)
{
    uint32_t table=D(0x1b124);
    if((int32_t)D(0x1b12c)>0)for(uint32_t i=0;(int32_t)i<=(int32_t)D(0x1b130);++i)if(!D(table+i*4))return i;
    return (int32_t)D(0x1b130)<(int32_t)D(0x1b128)?D(0x1b130):0xffffffff;
}
void kernel_ui_image_fill_812748(volatile uint32_t id,volatile uint32_t color)
{
    uint32_t table=D(0x1b124);if(!D(table+id*4))return;uint32_t image=D(table+id*4);
    if(D(image+64)==32){uint32_t pixels=D(image+28);for(uint32_t y=0;(int32_t)y<(int32_t)D(image+52);++y)for(uint32_t x=0;(int32_t)x<(int32_t)D(image+48);++x){D(pixels)=color;pixels+=4;}}
    else if(D(image+64)==8){uint32_t pixels=D(image+28);for(uint32_t y=0;(int32_t)y<(int32_t)D(image+52);++y)for(uint32_t x=0;(int32_t)x<(int32_t)D(image+48);++x){B(pixels)=color;++pixels;}}
}
void kernel_ui_image_release_api_812bfa(uint32_t id)
{kernel_ui_image_release_8125dc(id);}
