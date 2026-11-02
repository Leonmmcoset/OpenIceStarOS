/* Image controls, 0x8121d4..0x812748 and 0x81282e..0x812bfa.
 * Preserve packed owner lists, live metadata and the original failure paths,
 * including missing flag restoration/rollback and stale file-buffer fields. */
#include "image_controls.h"
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define B(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define P(a) ((uint32_t)(uintptr_t)(a))
#define OBJECT(id) D(0x6a0000u+4u*(id))
#define SURFACE(id) D(0x500000u+4u*(id))
uint32_t kernel_image_flags_81a24a(void);
void kernel_image_cli_81a242(void);
void kernel_image_restore_81a24d(uint32_t);
uint32_t kernel_image_alloc_8155ca(uint32_t,uint32_t);
void kernel_image_free_815978(uint32_t);
int32_t kernel_image_slot_81215a(void);
void kernel_image_copy_82a32b(uint32_t,uint32_t);
int32_t kernel_image_record_8263c0(uint32_t,uint32_t);
uint32_t kernel_image_read_827454(uint32_t);
void kernel_image_jpeg_8239a0(uint32_t,uint32_t,uint32_t);
void kernel_image_bmp_820d66(uint32_t,uint32_t);
void kernel_image_png_824c70(uint32_t,uint32_t,uint32_t);
void kernel_image_ico_82af4c(uint32_t,uint32_t);
void kernel_image_pixel32_81b2f4(uint32_t,uint32_t,uint32_t,uint32_t);
void kernel_image_pixel8_81b26e(uint32_t,uint32_t,uint32_t,uint32_t);
void kernel_image_text_81f88a(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);

static __attribute__((always_inline)) inline void decode(uint32_t image,uint32_t record)
{
    if(B(record+8)=='J' && B(record+9)=='P' && B(record+10)=='G')
        kernel_image_jpeg_8239a0(image+28,D(image+0x153),D(image+0x157));
    else if(B(record+8)=='B' && B(record+9)=='M' && B(record+10)=='P')
        kernel_image_bmp_820d66(image+28,D(image+0x153));
    else if(B(record+8)=='P' && B(record+9)=='N' && B(record+10)=='G')
        kernel_image_png_824c70(image+28,D(image+0x153),D(image+0x157));
    else if(B(record+8)=='I' && B(record+9)=='C' && B(record+10)=='O')
        kernel_image_ico_82af4c(image+28,D(image+0x153));
}

int32_t kernel_image_create_8121d4(volatile uint32_t owner,volatile uint32_t x,
    volatile uint32_t y,volatile uint32_t width,volatile uint32_t height,
    volatile int32_t mode,volatile uint32_t path)
{
    uint32_t table=D(0x1b124),object=OBJECT(owner);
    uint32_t flags=kernel_image_flags_81a24a();kernel_image_cli_81a242();
    uint32_t image=kernel_image_alloc_8155ca(0x15f,D(object+4));
    if(!image)return -1;
    int32_t id=kernel_image_slot_81215a();
    if(id<0){kernel_image_free_815978(image);return -1;}
    if((int32_t)D(object+0x15b)>254)return -1;
    D(image)=id;D(image+4)=owner;D(image+8)=D(object+0x15b);
    D(image+16)=1;D(image+12)=mode;
    if(path)kernel_image_copy_82a32b(image+84,path);else B(image+84)=0;
    D(object+0x15f+D(object+0x15b)*4u)=id;
    D(object+0x15b)=D(object+0x15b)+1u;
    D(image+32)=x;D(image+36)=y;D(image+40)=width;D(image+44)=height;
    D(image+64)=D(SURFACE(0)+36);
    if(mode==0 || mode==1){
        uint32_t bytes=width*height*(uint32_t)((int32_t)D(image+64)/8);
        D(image+28)=kernel_image_alloc_8155ca(bytes,D(object+4));
        D(image+48)=width;D(image+52)=height;
    }else if(mode>1){
        uint32_t bytes=D(SURFACE(0)+12)*D(SURFACE(0)+16)*(uint32_t)((int32_t)D(image+64)/8);
        D(image+28)=kernel_image_alloc_8155ca(bytes,D(object+4));
        D(image+48)=D(SURFACE(0)+12);D(image+52)=D(SURFACE(0)+16);
    }
    if(!D(image+28)){kernel_image_free_815978(image);return -1;}
    if(path){
        uint8_t record[32];
        if(kernel_image_record_8263c0(image+84,P(record))>0){
            D(image+0x153)=kernel_image_read_827454(P(record));
            if(!D(image+0x153)){
                kernel_image_free_815978(D(image+28));kernel_image_free_815978(image);return -1;
            }
            D(image+0x157)=D(P(record)+28);decode(image,P(record));
        }
    }
    D(table+(uint32_t)id*4u)=image;
    if((int32_t)D(0x1b130)>id)D(0x1b12c)=D(0x1b12c)-1u;
    else D(0x1b130)=D(0x1b130)+1u;
    kernel_image_restore_81a24d(flags);return id;
}

void kernel_image_release_8125dc(volatile uint32_t id)
{
    uint32_t flags=kernel_image_flags_81a24a();kernel_image_cli_81a242();
    uint32_t table=D(0x1b124);
    if(!D(table+id*4u))return;
    uint32_t image=D(table+id*4u),object=OBJECT(D(image+4));
    if((int32_t)D(object+0x15b)>0){
        if(D(image+0x153))kernel_image_free_815978(D(image+0x153));
        if(D(image+28))kernel_image_free_815978(D(image+28));
        if((int32_t)D(table+id*4u)>0)kernel_image_free_815978(D(table+id*4u));
        D(table+id*4u)=0;
        for(uint32_t i=0;(int32_t)i<(int32_t)D(object+0x15b);++i){
            if(D(object+0x15f+i*4u)!=id)continue;
            for(uint32_t j=i;(int32_t)j<(int32_t)D(object+0x15b);++j)
                D(object+0x15f+j*4u)=D(object+0x163+j*4u);
            break;
        }
        D(object+0x15b)=D(object+0x15b)-1u;
    }
    kernel_image_restore_81a24d(flags);
}

void kernel_image_file_81282e(volatile uint32_t id,volatile uint32_t path)
{
    uint32_t table=D(0x1b124);if(!D(table+id*4u))return;
    uint32_t image=D(table+id*4u);
    if(D(image+0x153))kernel_image_free_815978(D(image+0x153));
    kernel_image_copy_82a32b(image+84,path);
    uint8_t record[32];
    if(kernel_image_record_8263c0(image+84,P(record))>0){
        D(image+0x153)=kernel_image_read_827454(P(record));
        D(image+0x157)=D(P(record)+28);
        if(D(image+0x153))decode(image,P(record));
    }
}

void kernel_image_draw_8129ca(volatile uint32_t id)
{
    uint32_t theme=D(0x1b0e0),table=D(0x1b124),image=D(table+id*4u);
    uint32_t object=OBJECT(D(image+4));
    if(image==0xffffffacu || !B(image+84) || B(image+84)==' '){
        uint32_t y=(uint32_t)((int32_t)D(SURFACE(D(object+12))+16)/2);
        uint32_t x=(uint32_t)((int32_t)D(SURFACE(D(object+12))+12)/2)-40u;
        kernel_image_text_81f88a(D(object+12),x,y,D(theme),D(theme+32),0x83cd67);return;
    }
    if(D(SURFACE(D(object+12))+36)==32 && D(image+64)==32){
        for(uint32_t y=0;(int32_t)y<(int32_t)D(image+44);++y)
            for(uint32_t x=0;(int32_t)x<(int32_t)D(image+40);++x){
                uint32_t color=D(D(image+28)+(D(image+40)*y+x)*4u);
                kernel_image_pixel32_81b2f4(D(object+12),x+D(image+32),y+D(image+36),color);
            }
    }else if(D(SURFACE(D(object+12))+36)==8 && D(image+64)==8){
        for(uint32_t y=0;(int32_t)y<(int32_t)D(image+44);++y)
            for(uint32_t x=0;(int32_t)x<(int32_t)D(image+40);++x){
                uint32_t color=B(D(image+28)+D(image+40)*y+x);
                kernel_image_pixel8_81b26e(D(object+12),x+D(image+32),y+D(image+36),color);
            }
    }
}
