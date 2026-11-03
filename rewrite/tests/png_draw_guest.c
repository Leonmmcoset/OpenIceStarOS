/* Real FAT/PNG/DEFLATE/heap/pixel chains. Only the off-screen destination
 * metadata is a fixture; all called kernel functions execute normally. */
#include <stdint.h>
#define P(a) ((uint32_t)(uintptr_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
typedef uint32_t (*fn)();
#define CALL(a,...) ((fn)(a))(__VA_ARGS__)
static uint32_t surface[16];
static uint8_t pixels[16*12*4];
static const uint32_t colors[6]={0,0x112233,0x445566,0xff8020,0,0x112233};
static const uint8_t palette[6]={0,52,102,33,0,52};
static const int32_t offsets[3][2]={{0,0},{3,4},{-2,-1}};
static void emit(uint32_t v){for(uint32_t i=0;i<4;++i){__asm__ volatile("outb %0,%1"::"a"((uint8_t)v),"Nd"((uint16_t)0xe9));v>>=8;}}
static void reset(void){for(uint32_t i=0;i<sizeof(pixels);++i)pixels[i]=0x7e;}
static uint32_t check(uint32_t depth,int32_t left,int32_t top,uint32_t drawn)
{
    uint32_t used=16*12*(depth/8);
    for(uint32_t i=0;i<sizeof(pixels);++i){
        uint8_t expected=0x7e;
        if(drawn && i<used){
            uint32_t pixel=i/(depth/8);int32_t x=(int32_t)(pixel%16)-left,y=(int32_t)(pixel/16)-top;
            if(x>=0 && x<3 && y>=0 && y<2){
                uint32_t n=(uint32_t)(y*3+x);
                if(depth==32 && (n==2 || n==3))expected=(uint8_t)(colors[n]>>((i%4)*8));
                if(depth==8 && n!=0 && n!=4)expected=palette[n];
            }
        }
        if(pixels[i]!=expected)return 0;
    }
    return 1;
}
void fat_guest_entry(void)
{
    emit(0x504e4731);
    uint8_t record[32];emit((int32_t)CALL(0x8263c0,P("C:\\PNGTST.PNG"),P(record))>0);
    uint32_t length=D(P(record)+28);emit(length>0);
    uint32_t data=CALL(0x827454,P(record));emit(data!=0);
    uint32_t saved,old=D(0x500000+4095*4),services=D(0x1b0e4),oldpixel=D(services);
    __asm__ volatile("pushfl;popl %0;cli":"=r"(saved)::"memory");
    D(0x500000+4095*4)=P(surface);surface[0]=P(pixels);surface[3]=16;surface[4]=12;
    for(uint32_t mode=0;mode<2;++mode){
        uint32_t depth=mode?8:32;surface[9]=depth;
        /* ID dispatch selects the global video-mode entry, whereas direct
         * drawing selects by target depth. Select the actual 8/32-bit kernel
         * entries under CLI, then restore the production table below. */
        D(services)=mode?0x81b26e:0x81b2f4;
        for(uint32_t i=0;i<3;++i)for(uint32_t direct=0;direct<2;++direct){
            reset();CALL(direct?0x8241d0:0x824014,direct?P(surface):4095,offsets[i][0],offsets[i][1],data,length);
            emit(check(depth,offsets[i][0],offsets[i][1],1));
        }
        reset();CALL(0x82436c,4095,3,4,P("C:\\PNGTST.PNG"));emit(check(depth,3,4,1));
        reset();CALL(0x82436c,4095,3,4,P("ABSENT.PNG"));emit(check(depth,3,4,0));
        CALL(0x82436c,4095,3,4,P("C:\\ABSENT.PNG"));emit(check(depth,3,4,0));
    }
    D(0x500000+4095*4)=old;D(services)=oldpixel;
    __asm__ volatile("pushl %0;popfl"::"r"(saved):"cc","memory");
    CALL(0x815978,data);
    /* Display the same real file adapter in a normal window after restoring
     * the temporary table entry and CPU flags. */
    uint32_t w=CALL(0x807a0a,100,80,260,180,1,0,2,P("C PNG draw recovery"));emit((int32_t)w>0);
    uint32_t obj=D(0x6a0000+w*4),sid=D(obj+12),s=D(0x500000+sid*4);
    CALL(0x82436c,sid,30,40,P("C:\\PNGVIEW.PNG"));
    uint32_t ok=1;
    for(uint32_t y=0;y<64;++y)for(uint32_t x=0;x<96;++x){
        uint32_t c=y<32?(x<48?0xd04020:0xe0a020):(x<48?0x2060c0:0x30a050);
        if(D(D(s)+((y+40)*D(s+12)+x+30)*4)!=c)ok=0;
    }
    emit(ok);CALL(0x8082c6,w);emit(D(obj+0x123)==0);
    for(;;)CALL(D(D(0x1b0e4)+108*4));
}
