/* Original x86 / ordinary C / fixed-address C with a bytewise read oracle. */
#include "../kernel/pixel_sample.h"
#include "raster_fixture.h"
static uint32_t align,visited,pixels,hits,reasons[6];
static uint32_t descriptor(uint32_t j){return DATA+0x501u+j*128u+align;}
static void layer(uint32_t j,uint32_t left,uint32_t top,uint32_t width,uint32_t height,
                  uint32_t pixel,uint32_t key,uint32_t depth)
{
    uint32_t s=descriptor(j),address=DATA+0x2101u+j*64u+align;
    uint64_t index=(uint64_t)(args[2]-top)*width+(args[1]-left);
    REQUIRE(put(initial,s,address-(uint32_t)(index*4u),4),"fixture pixels");
    REQUIRE(put(initial,s+4u,left,4)&&put(initial,s+8u,top,4)&&put(initial,s+12u,width,4)&&
            put(initial,s+16u,height,4)&&put(initial,s+36u,depth,4)&&put(initial,s+40u,key,4)&&
            put(initial,address,pixel,4),"fixture layer");
}
static void setup(uint32_t position,uint32_t id,uint32_t x,uint32_t y,uint32_t codes)
{
    pattern();args[0]=id;args[1]=x;args[2]=y;
    for(uint32_t i=3;i<6;++i)args[i]=random32();
    uint32_t subject=DATA+0x101u+align;
    REQUIRE(put(initial,0x500000u+id*4u,subject,4)&&put(initial,subject+44u,position,4),"fixture subject");
    for(uint32_t j=0;j<3;++j){
        uint32_t code=(codes>>(j*3u))&7u,left=x-1u,top=y-1u,width=3,height=3;
        uint32_t pixel=0x12345678u+j*0x3456789u+cases,key=pixel^0x55005500u;
        if(code==1)left=x;
        if(code==2)width=1;
        if(code==3)top=y;
        if(code==4)height=1;
        if(code==5)key=pixel;
        if(code==6){pixel=UINT32_MAX;key=0;}
        if(code==7)width=0x80000000u;
        uint32_t other=16u+j+0x40000000u*((cases+j)%4u);
        REQUIRE(put(initial,0x510000u+(position-1u-j)*4u,other,4)&&
                put(initial,0x500000u+other*4u,descriptor(j),4),"fixture order");
        layer(j,left,top,width,height,pixel,key,values[(codes+j)%16u]);
    }
}
static uint32_t oracle(void)
{
    uint32_t subject=get32(initial,0x500000u+args[0]*4u);
    uint32_t n=get32(initial,subject+44u)-1u,steps=0;
    while(signed32(n)>0){
        REQUIRE(++steps<=8,"fixture traversal cap");++visited;
        uint32_t id=get32(initial,0x510000u+n*4u),s=get32(initial,0x500000u+id*4u);
        uint32_t left=get32(initial,s+4u);
        if(signed32(left)>=signed32(args[1])){++reasons[0];--n;continue;}
        uint32_t width=get32(initial,s+12u),right=(uint32_t)((uint64_t)left+width);
        if(signed32(right)<=signed32(args[1])){++reasons[1];--n;continue;}
        uint32_t top=get32(initial,s+8u);
        if(signed32(top)>=signed32(args[2])){++reasons[2];--n;continue;}
        uint32_t bottom=(uint32_t)((uint64_t)top+get32(initial,s+16u));
        if(signed32(bottom)<=signed32(args[2])){++reasons[3];--n;continue;}
        uint64_t index=(uint64_t)(args[2]-top)*width+(args[1]-left);
        uint32_t address=(uint32_t)((uint64_t)get32(initial,s)+index*4u);
        uint32_t pixel=get32(initial,address);++pixels;
        if(pixel!=get32(initial,s+40u)){++hits;return pixel;}
        ++reasons[4];--n;
    }
    ++reasons[5];return UINT32_MAX;
}
static void run(void)
{
    scenario=cases;begin_oracle();uint32_t value=oracle();
    const void *functions[]={(void *)0x91b514,kernel_pixel_sample_below_81b514,(void *)0x81b514};
    for(implementation=0;implementation<3;++implementation){
        reset();uint32_t actual[7];
        REQUIRE(!raster_checked_call(functions[implementation],args,actual),"sampler ABI mismatch");
        compare(actual,value);
    }
    ++cases;
}
void _start(void)
{
    mappings();REQUIRE(load("build/raster_original.x",0x900000),"original load");
#ifdef TEST_INSTALLED
    REQUIRE(load("build/icestar_c.x",0x800000),"installed load");
#else
    REQUIRE(load("build/raster_preview.x",0x800000),"preview load");
#endif
    stage=1;
    for(align=0;align<4;++align)for(uint32_t xy=0;xy<12;++xy)for(uint32_t codes=0;codes<512;++codes){
        setup(4,ids[(xy+codes)%16u],values[xy],values[(xy+3u)%16u],codes);run();
    }
    stage=2;
    for(uint32_t w=0;w<16;++w)for(uint32_t h=0;h<16;++h)
      for(uint32_t x=0;x<16;++x)for(uint32_t y=0;y<16;++y){
        align=(w+h+x+y)%4u;setup(2,ids[(w+x)%16u],values[x],values[y],0);
        uint32_t pixel=random32();
        layer(0,values[(w+x)%16u],values[(h+y)%16u],values[w],values[h],
              pixel,(x+y)%3u?pixel^0xff000000u:pixel,values[(w+h)%16u]);run();
      }
    stage=3;
    static const uint32_t positions[]={0,1,2,3,4,0x40000000,0x40000002,
        0x7fffffff,0x80000000,0x80000001,0xc0000002,0xffffffff};
    for(align=0;align<4;++align)for(uint32_t p=0;p<12;++p)for(uint32_t i=0;i<16;++i){
        setup(positions[p],ids[i],3,4,0);run();
    }
    stage=4;
    for(uint32_t guard=0;guard<6;++guard)for(uint32_t id=0;id<16;++id){
        align=0;setup(2,ids[id],3,4,0);
        if(guard<4){
            uint32_t s=DATA+16384u-(guard==0?8u:guard==3?20u:16u);
            REQUIRE(put(initial,0x500040u,s,4)&&put(initial,s,0x17283940u,4),"guard surface");
            REQUIRE(put(initial,s+4u,guard?2u:3u,4),"guard left");
            if(guard)REQUIRE(put(initial,s+8u,guard==2?4u:3u,4)&&put(initial,s+12u,guard==1?1u:3u,4),"guard right/top");
            if(guard==3)REQUIRE(put(initial,s+16u,1,4),"guard bottom");
        }else{
            uint32_t s=DATA+16384u-48u;
            REQUIRE(put(initial,0x500000u+ids[id]*4u,s,4)&&put(initial,s+44u,guard-4u,4),"guard subject");
        }
        run();
    }
    PRINT("Pixel sampler cases: ");number(cases);PRINT("; executions: ");number(cases*3u);
    PRINT("; visited/pixel reads/opaque hits: ");number(visited);PRINT(" ");number(pixels);PRINT(" ");number(hits);
    PRINT("; left/right/top/bottom/key/exhausted: ");for(uint32_t i=0;i<6;++i){number(reasons[i]);PRINT(" ");}
    PRINT("; excluded scenarios: ");number(skipped);
    PRINT("; sampler verified against 32 KiB memory/return/ABI oracle; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
