/* Image controls exercised through real FAT, heap, codecs and surface pixels. */
#include <stdint.h>
#define P(a) ((uint32_t)(uintptr_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
typedef uint32_t (*fn)();
#define CALL(a,...) ((fn)(a))(__VA_ARGS__)
static const char *paths[]={"C:\\IMGTST.PNG","C:\\IMGTST.BMP","C:\\IMGTST.JPG","C:\\IMGTST.ICO"};
static const uint32_t colors[]={0x112233,0x445566,0x808080,0x778899};
static volatile uint32_t emitted_words;
static void emit(uint32_t v){for(uint32_t i=0;i<4;++i){__asm__ volatile("outb %0,%1"::"a"((uint8_t)v),"Nd"((uint16_t)0xe9));v>>=8;}++emitted_words;}
static uint32_t flags(void){uint32_t v;__asm__ volatile("pushfl;popl %0":"=r"(v));return v;}
static void restore(uint32_t v){__asm__ volatile("pushl %0;popfl"::"r"(v):"cc","memory");}
static uint32_t uniform(uint32_t p,uint32_t stride,uint32_t which)
{
    for(uint32_t y=0;y<8;++y)for(uint32_t x=0;x<8;++x){
        uint32_t v=D(p+(y*stride+x)*4);
        if(which==2){
            for(uint32_t c=0;c<3;++c){uint32_t b=(v>>(c*8))&255;if(b<127 || b>129)return 0;}
        }else if(v!=colors[which])return 0;
    }
    return 1;
}
void fat_guest_entry(void)
{
    emit(0x494d4731);
    uint32_t w=CALL(0x807a0a,100,80,260,180,1,0,2,P("C image controls"));emit((int32_t)w>0);
    uint32_t obj=D(0x6a0000+w*4),sid=D(obj+12),surface=D(0x500000+sid*4);
    uint32_t before=D(obj+0x15b);
    uint32_t im=CALL(0x8121d4,w,30,40,8,8,0,0);emit((int32_t)im>=0);
    uint32_t image=D(D(0x1b124)+im*4);emit(D(image)==im && D(image+4)==w);
    emit(D(image+32)==30 && D(image+36)==40 && D(image+40)==8 && D(image+44)==8);
    emit(D(image+48)==8 && D(image+52)==8 && D(image+64)==32);
    emit(D(obj+0x15b)==before+1 && D(obj+0x15f+before*4)==im);
    CALL(0x8129ca,im); /* Real placeholder text branch. */
    for(uint32_t i=0;i<4;++i){
        CALL(0x81282e,im,P(paths[i]));
        emit(D(image+0x153)!=0);emit(uniform(D(image+28),8,i));
        CALL(0x8129ca,im);
        emit(uniform(D(surface)+(40*D(surface+12)+30)*4,D(surface+12),i));
    }
    CALL(0x8125dc,im);emit(D(D(0x1b124)+im*4));emit(D(obj+0x15b)==before);
    /* Real create-with-path branch, its decoder, and child-list compaction. */
    uint32_t a=CALL(0x8121d4,w,50,40,8,8,1,P(paths[0]));emit((int32_t)a>=0);
    image=D(D(0x1b124)+a*4);emit(uniform(D(image+28),8,0));
    uint32_t b=CALL(0x8121d4,w,70,40,8,8,2,P(paths[1]));emit((int32_t)b>=0);
    image=D(D(0x1b124)+b*4);emit(D(image+48)==1024 && D(image+52)==768);
    CALL(0x8125dc,a);emit(D(obj+0x15b)==before+1 && D(obj+0x15f+before*4)==b);
    CALL(0x8125dc,b);emit(D(obj+0x15b)==before);
    /* Observe the original empty-slot CLI early return, then restore the
     * caller's real flags so the diagnostic can continue scheduling. */
    uint32_t empty=CALL(0x81215a),saved=flags();emit((saved&0x200)!=0);
    emit((int32_t)empty>=0 && D(D(0x1b124)+empty*4)==0);
    CALL(0x8125dc,empty);uint32_t after=flags();restore(saved);emit((after&0x200)==0);
    /* Visible production window uses the same recovered create/draw path. */
    CALL(0x837545,sid,3,24,256,176,0xc6c6c6);
    im=CALL(0x8121d4,w,30,40,96,64,0,P(paths[0]));emit((int32_t)im>=0);
    image=D(D(0x1b124)+im*4);emit(uniform(D(image+28),96,0));
    CALL(0x8129ca,im);emit(uniform(D(surface)+(40*D(surface+12)+30)*4,D(surface+12),0));
    CALL(0x8082c6,w);
    for(;;)CALL(D(D(0x1b0e4)+108*4));
}
