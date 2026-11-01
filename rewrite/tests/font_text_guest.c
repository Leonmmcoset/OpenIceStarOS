/* Real X diagnostic; no implementation helpers replaced. */
#include <stdint.h>
#define P(a) ((uint32_t)(uintptr_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(a))
typedef uint32_t (*fn)();
#define CALL(a,...) ((fn)(a))(__VA_ARGS__)
static uint8_t font[4096],pixels[8192];
static uint32_t info[40],surface[16],dims[4];
static const uint8_t utf[]={0,'O',0,'p',0,'e',0,'n',0,'I',0,'c',0,'e'};
static void emit(uint32_t value){for(uint32_t i=0;i<4;++i){__asm__ volatile("outb %0,%1"::"a"((uint8_t)value),"Nd"((uint16_t)0xe9));value>>=8;}}
static void be16(uint32_t off,uint32_t v){font[off]=v>>8;font[off+1]=v;}
static void be32(uint32_t off,uint32_t v){be16(off,v>>16);be16(off+2,v);}
static void table(uint32_t i,uint32_t tag,uint32_t off,uint32_t len){be32(12+i*16,tag);be32(20+i*16,off);be32(24+i*16,len);}
static void reset(void){for(uint32_t i=0;i<sizeof(pixels);++i)pixels[i]=0x5a;}
static uint32_t hash(void){uint32_t h=2166136261;for(uint32_t i=0;i<sizeof(pixels);++i)h=(h^pixels[i])*16777619;return h;}
static void initialize(void)
{
    be32(0,0x10000);be16(4,8);
    table(0,0x636d6170,256,274);table(1,0x6c6f6361,1024,16);
    table(2,0x68656164,1280,64);table(3,0x676c7966,1536,44);
    table(4,0x68686561,2048,40);table(5,0x686d7478,2304,12);table(6,0x6d617870,2560,6);
    table(7,0x6e616d65,2816,128);
    be16(2818,3);be16(2820,42);
    const char *names[3]={"OpenIce","Regular","OpenIce Regular"};
    uint32_t pos=0;
    for(uint32_t i=0;i<3;++i){uint32_t r=2822+i*12,n=0;while(names[i][n])++n;
        be16(r,3);be16(r+2,1);be16(r+4,1033);be16(r+6,i==0?16:i==1?17:3);be16(r+8,n*2);be16(r+10,pos);
        for(uint32_t j=0;j<n;++j){font[2858+pos++]=0;font[2858+pos++]=names[i][j];}}
    be16(258,1);be16(260,3);be16(262,1);be32(264,12);
    be16(270,262);font[274+65]=1;font[274+66]=2;
    be32(1028,22);be32(1032,44);be32(1036,44);
    be16(1298,16);be16(1330,1);
    const uint8_t glyph[]={0,1,0,1,0,0,0,7,0,4,0,3,0,0,59,1,53,35,1,6,6,4};
    for(uint32_t i=0;i<sizeof(glyph);++i){font[1536+i]=glyph[i];font[1558+i]=glyph[i];}
    be16(2052,12);be16(2054,0xfffc);be16(2056,2);be16(2082,3);
    be16(2304,10);be16(2306,1);be16(2308,12);be16(2310,1);be16(2312,8);be16(2564,3);
}
void fat_guest_entry(void)
{
    emit(0x54455831);initialize();emit(CALL(0x82a2c5,P(font),P(info)));
    emit(CALL(0x8365b2,P("OpenIce"),7,P(utf),14));
    emit(CALL(0x83689a,P("OpenIce"),7,P(utf),14));
    emit(CALL(0x83702e,P("OpenIce"),7,P(utf),14));
    emit(CALL(0x8368d8,P(info),P(dims),3,1,1033,16)-P(font));emit(dims[0]);
    emit(CALL(0x836a34,P(font),2816,P("OpenIce Regular"),15,16,17));
    emit(CALL(0x836cc0,P(font),0,P("OpenIce Regular"),0));
    emit(CALL(0x836f08,P(font),P("OpenIce Regular"),0));
    emit(CALL(0x83700c,P(font),P("OpenIce Regular"),0));
    emit(CALL(0x8293e9,0));emit(CALL(0x8296d7,0x80));
    emit(CALL(0x829707,P("\xf0\x9f\x98\x80"),P(dims)));emit(dims[0]);
    uint32_t id=4095,old=D(0x500000+id*4);D(0x500000+id*4)=P(surface);
    surface[0]=P(pixels);surface[3]=64;surface[4]=32;surface[9]=32;surface[10]=0xff00ff;
    reset();CALL(0x829003,P(info),0x41800000,65,id,4,8,0xe15397,0x173759,P(dims),P(dims+1),P(dims+2),P(dims+3));
    for(uint32_t i=0;i<4;++i){emit(dims[i]);}emit(hash());
    reset();CALL(0x829133,P(info),0x41800000,65,id,4,8,7,5,0xe15397,0x173759,P(dims),P(dims+1),P(dims+2),P(dims+3));emit(hash());
    reset();CALL(0x82927d,P(info),0x41800000,65,id,4,8,0xe15397,P(dims),P(dims+1),P(dims+2),P(dims+3));emit(hash());
    reset();CALL(0x829485,P(info),0x41800000,P("A"),id,4,8,0xe15397,0x173759,1);emit(hash());
    reset();CALL(0x829941,P(info),0x41800000,P("A"),id,4,8,0xe15397,0x173759,1);emit(hash());
    reset();emit(CALL(0x829b4d,P(info),0x41800000,P("A"),id,4,8,64,32,0xe15397,0x173759,1,8,1));emit(hash());
    reset();CALL(0x829dd3,P(info),0x41800000,P("A"),id,4,8,7,5,0xe15397,0x173759,1);emit(hash());
    reset();CALL(0x82a03f,P(info),0x41800000,P("A"),id,4,8,0xe15397,1);emit(hash());
    D(0x500000+id*4)=old;
    for(;;)CALL(D(D(0x1b0e4)+108*4));
}
