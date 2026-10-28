/* Model octants and side-specific checks independently from the C bodies.
 * Each store updates the model's live caller and descriptor memory. */
#include "../kernel/raster_circle.h"
#include "raster_fixture.h"
static uint32_t format,alignment,counts[2],store_count[2],caller_stores,
    metadata_stores,asymmetric_stores,negative_radii;
static int valid(uint32_t address,unsigned bytes)
{
    for(unsigned i=0;i<bytes;++i)if(index_of(address+i)==UINT32_MAX)return 0;
    return 1;
}
static void setup(uint32_t id,uint32_t width,uint32_t height,uint32_t x,
    uint32_t y,uint32_t radius,uint32_t destination,uint32_t color)
{
    pattern();args[0]=id;args[1]=x;args[2]=y;args[3]=radius;args[4]=color;args[5]=0x8badf00d;
    uint32_t surface=DATA+0x101u+alignment,other=DATA+0x201u+alignment;
    uint64_t first=(uint64_t)(y+radius-1u)*width+(x-1u);
    REQUIRE(put(initial,0x500000u+id*4u,surface,4)&&
        put(initial,surface,destination-(uint32_t)(first*(format?4u:1u)),4)&&
        put(initial,surface+12u,width,4)&&put(initial,surface+16u,height,4),"circle surface");
    if(id!=1u)REQUIRE(put(initial,0x500004u,other,4),"secondary slot");
    REQUIRE(put(initial,other,DATA+0x2801u+alignment,4)&&
        put(initial,other+12u,8,4)&&put(initial,other+16u,8,4),"secondary surface");
}
static int surface(uint32_t *record)
{
    uint32_t id=get32(expected,ARGUMENTS),slot=0x500000u+id*4u;
    if(!valid(slot,4))return 0;
    *record=get32(expected,slot);return 1;
}
static int point(uint32_t x,uint32_t y,unsigned positive_x,unsigned positive_y)
{
    uint32_t record,width,height;
    if(positive_x){
        if(!surface(&record)||!valid(record+12u,4))return 0;
        width=get32(expected,record+12u);
        if(signed32(x)>=signed32(width))return 1;
    }else if(signed32(x)<0)return 1;
    if(positive_y){
        if(!surface(&record)||!valid(record+16u,4))return 0;
        height=get32(expected,record+16u);
        if(signed32(y)>=signed32(height))return 1;
    }else if(signed32(y)<0)return 1;
    if(!surface(&record)||!valid(record,4)||!valid(record+12u,4))return 0;
    uint64_t offset=(uint64_t)y*get32(expected,record+12u)+x;
    uint32_t address=(uint32_t)((uint64_t)get32(expected,record)+offset*(format?4u:1u));
    if(!put(expected,address,get32(expected,ARGUMENTS+16u),format?4u:1u))return 0;
    ++store_count[format];
    if(address-ARGUMENTS<24u)++caller_stores;
    if(address-DATA<0x1000u||address-0x500000u<4096u)++metadata_stores;
    if(signed32(x)<0||signed32(y)<0)++asymmetric_stores;
    return 1;
}
static int oracle_body(uint32_t *result)
{
    int64_t radius=signed32(args[3]);
    if(radius<0){*result=args[3];++negative_radii;return 1;}
    if(radius>64)return 0;
    int64_t x=0,y=radius,error=-(radius/2);
    while(y>=x){
        uint32_t cx=get32(expected,ARGUMENTS+4u),cy=get32(expected,ARGUMENTS+8u);
        uint32_t xx[4]={cx+(uint32_t)x-1u,cx+(uint32_t)y-1u,cx-(uint32_t)x,cx-(uint32_t)y};
        uint32_t yy[4]={cy+(uint32_t)y-1u,cy+(uint32_t)x-1u,cy-(uint32_t)y,cy-(uint32_t)x};
        /* X/Y index pairs and the side of each coordinate, in store order. */
        static const uint8_t octants[8][4]={
            {0,0,1,1},{1,1,1,1},{2,0,0,1},{3,1,0,1},
            {2,2,0,0},{3,3,0,0},{0,2,1,0},{1,3,1,0}};
        for(unsigned p=0;p<8;++p)
            if(!point(xx[octants[p][0]],yy[octants[p][1]],octants[p][2],octants[p][3]))return 0;
        ++x;
        if(error>=0){--y;error-=y;}
        error+=x;
    }
    *result=(uint32_t)y;return 1;
}
static int oracle(uint32_t *result)
{
    uint32_t *counters[]={&store_count[0],&store_count[1],&caller_stores,
        &metadata_stores,&asymmetric_stores,&negative_radii},saved[6];
    for(unsigned i=0;i<6;++i)saved[i]=*counters[i];
    if(oracle_body(result))return 1;
    for(unsigned i=0;i<6;++i)*counters[i]=saved[i];
    return 0;
}
static void run(void)
{
    scenario=cases+skipped;begin_oracle();uint32_t value;
    if(!oracle(&value)){++skipped;return;}
    const void *functions[]={format?(void *)0x91c6f2:(void *)0x91c396,
        format?(void *)kernel_raster_circle32_81c6f2:(void *)kernel_raster_circle8_81c396,
        format?(void *)0x81c6f2:(void *)0x81c396};
    for(implementation=0;implementation<3;++implementation){
        reset();uint32_t actual[7];
        REQUIRE(!raster_checked_call(functions[implementation],args,actual),"circle ABI mismatch");
        compare(actual,value);
    }
    ++cases;++counts[format];
}
void _start(void)
{
    mappings();REQUIRE(load("build/raster_circle_original.x",0x900000)&&
        load("build/raster_circle_candidate.x",0x800000),"circle image load");
    stage=1;
    for(format=0;format<2;++format)for(alignment=0;alignment<4;++alignment)
      for(unsigned cx=0;cx<7;++cx)for(unsigned cy=0;cy<7;++cy)for(unsigned radius=0;radius<17;++radius){
        setup(ids[(cx+cy+radius)%16u],31,23,cx*5u-8u,cy*5u-8u,radius,
            DATA+0x2801u+alignment,random32());run();
      }
    stage=2;
    for(format=0;format<2;++format)for(alignment=0;alignment<4;++alignment)
      for(unsigned w=0;w<16;++w)for(unsigned h=0;h<16;++h)for(unsigned radius=0;radius<4;++radius){
        setup(ids[(w+h+radius)%16u],values[w],values[h],3,4,radius,
            DATA+0x2801u+alignment,random32());run();
      }
    stage=3;
    for(format=0;format<2;++format)for(alignment=0;alignment<4;++alignment)
      for(unsigned cx=0;cx<16;++cx)for(unsigned cy=0;cy<16;++cy)for(unsigned r=0;r<16;++r){
        uint32_t radius=values[r];
        if((int32_t)radius>64)continue;
        setup(ids[(cx+cy+r)%16u],values[(cx+cy)%16u],values[(cx+cy+r)%16u],values[cx],values[cy],radius,
            DATA+0x2801u+alignment,random32());run();
      }
    stage=4;
    for(format=0;format<2;++format)for(alignment=0;alignment<4;++alignment)
      for(unsigned radius=0;radius<3;++radius)for(unsigned offset=0;offset<24;++offset){
        setup(0,0,8,1,1,radius,ARGUMENTS+offset,0);run();
      }
    for(format=0;format<2;++format)for(alignment=0;alignment<4;++alignment)
      for(unsigned a=0;a<9;++a){
        static const uint32_t offsets[]={0,4,8,12,16,40};
        uint32_t dest=a<6u?DATA+0x101u+alignment+offsets[a]:0x500000u+(a-6u)*4u;
        uint32_t color=a==0?DATA+0x2801u+alignment:a>=6u?DATA+0x201u+alignment:4u;
        setup(0,3,3,1,1,0,dest,color);run();
      }
    stage=5;
    for(format=0;format<2;++format)for(alignment=0;alignment<4;++alignment)
      for(unsigned color=0;color<256;++color){
        setup(0,31,23,3,4,color%5u,DATA+0x2801u+alignment,
            color|((color^255u)<<8)|((color^0x69u)<<16)|((color^0x96u)<<24));run();
      }
    stage=6;
    for(format=0;format<2;++format){
        alignment=0;setup(0,0x80000000u,0,-8u,-8u,2,DATA+0x2801u,random32());
        uint32_t record=DATA+16384u-16u;
        REQUIRE(put(initial,0x500000u,record,4)&&put(initial,record+12u,0x80000000u,4),"guard record");run();
    }
    REQUIRE(caller_stores&&metadata_stores&&asymmetric_stores&&negative_radii,"missing circle coverage");
    PRINT("Circle cases8/32: ");number(counts[0]);PRINT(" ");number(counts[1]);
    PRINT("; executions: ");number(cases*3u);PRINT("; excluded large/unmapped scenarios: ");number(skipped);
    PRINT("; stores8/32: ");number(store_count[0]);PRINT(" ");number(store_count[1]);
    PRINT("; caller/metadata/asymmetric stores: ");number(caller_stores);PRINT(" ");number(metadata_stores);
    PRINT(" ");number(asymmetric_stores);PRINT("; negative radii: ");number(negative_radii);
    PRINT("; circle outlines verified against 32 KiB memory/return/live-argument/ABI oracle; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
