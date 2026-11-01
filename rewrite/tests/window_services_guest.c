/* Real guest diagnostic. Kernel callbacks, task creation, heap, drawing and
 * compositor all execute normally. Temporary table fixtures are restored. */
#include <stdint.h>
#define P(a) ((uint32_t)(uintptr_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
typedef uint32_t (*fn)();
#define CALL(a,...) ((fn)(a))(__VA_ARGS__)
static uint32_t object[384],surface[16],image[88],state[4],args[4];
static uint8_t pixels[256*192*4],mail[256];
static void emit(uint32_t v){for(uint32_t i=0;i<4;++i){__asm__ volatile("outb %0,%1"::"a"((uint8_t)v),"Nd"((uint16_t)0xe9));v>>=8;}}
static void poll(void){CALL(D(D(0x1b0e4)+108*4));}
static void flags(uint32_t v){__asm__ volatile("pushl %0;popfl"::"r"(v):"cc","memory");}
static uint32_t lock(void){uint32_t v;__asm__ volatile("pushfl;popl %0;cli":"=r"(v)::"memory");return v;}
static uint32_t hash(void){uint32_t h=2166136261;for(uint32_t i=0;i<sizeof(pixels);++i)h=(h^pixels[i])*16777619;return h;}
static void reset(void){for(uint32_t i=0;i<sizeof(pixels);++i)pixels[i]=0x7e;}
static void cb4(uint32_t id,uint32_t a,uint32_t b,uint32_t c){args[0]=id;args[1]=a;args[2]=b;args[3]=c;}
static void cb3(uint32_t id,uint32_t a,uint32_t b){args[0]=id;args[1]=a;args[2]=b;}
static void redraw(uint32_t id){++state[3];args[0]=id;}
static uint32_t wait_mail(void){for(uint32_t n=0;n<200000;++n){uint32_t busy=0;for(uint32_t i=0;i<256;++i)busy|=*(volatile uint8_t *)(uintptr_t)(D(0x1b134)+i);if(!busy)return 1;poll();}return 0;}
void fat_guest_entry(void)
{
    emit(0x57494e31);
    CALL(0x802e64,P("mail"));CALL(0x802ec6,P(state),P(mail));emit(state[0]);emit(D(P(mail)));emit(mail[4]);
    for(uint32_t i=0;i<256;++i)*(volatile uint8_t *)(uintptr_t)(D(0x1b134)+i)=0;
    CALL(0x802ef2,P("C:\\NOFILE.X"));emit(wait_mail());
    CALL(0x802f42,P("C:\\NOFILE.X"));emit(wait_mail());
    uint32_t saved=lock(),id=4095,oldobj=D(0x6a0000+id*4),oldsurf=D(0x500000+id*4);
    D(0x6a0000+id*4)=P(object);D(0x500000+id*4)=P(surface);
    D(P(object)+12)=id;D(P(object)+0x113)=0;D(P(object)+0x14b)=3;D(P(object)+0x14f)=24;D(P(object)+0x153)=180;D(P(object)+0x157)=140;
    surface[0]=P(pixels);surface[1]=10;surface[2]=20;surface[3]=256;surface[4]=192;surface[9]=32;surface[10]=255;
    emit(CALL(0x80762e,id,14,24));emit(CALL(0x80762e,id,13,24));
    reset();emit(CALL(0x8076d0,id,100,100));emit(CALL(0x8076d0,id,13,100));
    emit(CALL(0x80784a,id,11,21));emit(CALL(0x80784a,id,10,20));
    reset();CALL(0x807302,id,8,9,17,12,0x35);emit(hash());
    reset();CALL(0x811e4c,id,8,9,25,21);emit(hash());
    CALL(0x807540,id,1); /* unsupported style 0: no drawing */
    uint32_t oldslot=D(0x6a0000),oldfree=D(0x1b094),oldcount=D(0x1b098);D(0x6a0000)=0;D(0x1b094)=1;D(0x1b098)=0;emit(CALL(0x807990));
    D(0x6a0000)=P(object);D(0x1b098)=1;D(P(object)+0x573)=P(redraw);CALL(0x80fb12);emit(state[3]);emit(args[0]);D(P(object)+0x573)=0;
    D(0x6a0000)=oldslot;D(0x1b094)=oldfree;D(0x1b098)=oldcount;
    D(P(object)+0x583)=P(cb4);D(P(object)+0x58b)=P(cb3);D(P(object)+0x593)=P(cb3);D(P(object)+0x597)=P(cb3);
    CALL(0x809e24,id,11,22,33);for(uint32_t i=0;i<4;++i){emit(args[i]);}
    CALL(0x80ac4e,id,44,55);emit(args[1]);emit(args[2]);
    CALL(0x80ecba,id,66,77);emit(args[1]);emit(args[2]);
    CALL(0x80ed08,id,88,99);emit(args[1]);emit(args[2]);
    uint32_t imagetable=D(0x1b124),oldimage=D(imagetable+id*4),oldifree=D(0x1b12c),oldicount=D(0x1b130);
    D(imagetable+id*4)=P(image);D(P(image)+28)=P(pixels);D(P(image)+48)=12;D(P(image)+52)=9;D(P(image)+64)=32;
    reset();CALL(0x812748,id,0x12345678);emit(hash());
    uint32_t oldzero=D(imagetable);D(imagetable)=0;D(0x1b12c)=1;D(0x1b130)=0;emit(CALL(0x81215a));D(imagetable)=oldzero;
    D(imagetable+id*4)=oldimage;D(0x1b12c)=oldifree;D(0x1b130)=oldicount;
    D(0x6a0000+id*4)=oldobj;D(0x500000+id*4)=oldsurf;flags(saved);
    /* Real window allocation and actual drawing/refresh/order services. */
    uint32_t w=CALL(0x807a0a,100,80,260,180,1,0,2,P("C window recovery"));emit((int32_t)w>0);
    uint32_t obj=D(0x6a0000+w*4),sid=D(obj+12),surf=D(0x500000+sid*4);
    CALL(0x808090,w,0,99);emit(D(obj+0x56b));
    CALL(0x807ffe,w,P("C:\\NOICON.ICO"),1);emit(D(obj+0x56b));
    CALL(0x8082c6,w);emit(D(obj+0x123));emit(D(surf+44)>0);
    CALL(0x807540,w,1);emit(CALL(0x8078f2,110,110)==w);
    CALL(0x80ac9c,w,3,4);emit(D(surf+4));emit(D(surf+8));emit(D(obj+0x12b));emit(D(obj+0x12f));
    CALL(0x8081ea,w);emit(D(obj+0x123));emit(D(surf+44));
    CALL(0x8080ea,w);emit(D(obj+0x123));emit(D(surf+44)>0);
    CALL(0x81002e,w);emit(D(obj+0x123));emit(D(surf+44));
    CALL(0x8082c6,w);CALL(0x808378,w);emit(D(obj+0x123));emit(D(surf+44));
    /* Actual image allocation, fill, release and owner child-list removal. */
    uint32_t im=CALL(0x8121d4,w,0,0,12,9,0,0);emit((int32_t)im>=0);CALL(0x812748,im,0xabcdef);CALL(0x812bfa,im);emit(D(D(0x1b124)+im*4));
    CALL(0x810168,w);emit(D(0x6a0000+w*4));
    w=CALL(0x807a0a,100,80,260,180,1,0,2,P("C dialog recovery"));obj=D(0x6a0000+w*4);D(obj+0x15b)=1;
    CALL(0x811c46,w,200,190);emit(D(0x6a0000+w*4));
    for(;;)poll();
}
