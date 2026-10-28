/* Independent bytewise oracle: signed geometry is widened before clipping;
 * wrapping occurs explicitly at DWORD boundaries. Copy service targets remain
 * real scalar/SSE code, including forward DWORD / simultaneous 16-byte reads.
 */
#include "../kernel/raster_fill.h"
#include "raster_fixture.h"
#define SERVICES 0x620000u
static uint32_t format,align,accelerated,counts[2],stores[2],copies[2],
    block_stores,tail_stores,metadata_stores,caller_stores,negative_clips,clamps;
static uint8_t direct_saved_services[4096];
static int valid(uint32_t a,uint32_t bytes)
{for(uint32_t i=0;i<bytes;++i)if(index_of(a+i)==UINT32_MAX)return 0;return 1;}
static uint32_t clipped(uint32_t value,uint32_t dimension)
{
    int64_t v=signed32(value),bound=signed32((uint32_t)((uint64_t)dimension-1u));
    if(v<0){v=0;++clamps;}
    if(v>bound){v=bound;++clamps;}
    if(v<0)++negative_clips;
    return (uint32_t)v;
}
static void geometry(uint32_t *xmin,uint32_t *xmax,uint32_t *ymin,uint32_t *ymax)
{
    uint32_t s=get32(expected,0x500000u+args[0]*4u);
    uint32_t w=get32(expected,s+12u),h=get32(expected,s+16u);
    uint32_t xlo=signed32(args[1])<signed32(args[3])?args[1]:args[3];
    uint32_t xhi=signed32(args[1])>signed32(args[3])?args[1]:args[3];
    uint32_t ylo=signed32(args[2])<signed32(args[4])?args[2]:args[4];
    uint32_t yhi=signed32(args[2])>signed32(args[4])?args[2]:args[4];
    *xmin=clipped(xlo,w);*xmax=clipped(xhi,w);
    *ymin=clipped(ylo,h);*ymax=clipped(yhi,h);
}
static void setup(uint32_t id,uint32_t width,uint32_t height,uint32_t x0,uint32_t y0,
                  uint32_t x1,uint32_t y1,uint32_t destination,uint32_t color)
{
    pattern();args[0]=id;args[1]=x0;args[2]=y0;args[3]=x1;args[4]=y1;args[5]=color;
    uint32_t surface=DATA+0x101u+align,other=DATA+0x201u+align;
    REQUIRE(put(initial,0x500000u+id*4u,surface,4)&&
        put(initial,surface+12u,width,4)&&put(initial,surface+16u,height,4),"fixture dimensions");
    begin_oracle();uint32_t xmin,xmax,ymin,ymax;
    uint32_t saved_negative=negative_clips,saved_clamps=clamps;
    geometry(&xmin,&xmax,&ymin,&ymax);
    negative_clips=saved_negative;clamps=saved_clamps;
    uint32_t index=(uint32_t)((uint64_t)ymin*width+xmin);
    uint32_t pixels=destination-index*(format?4u:1u);
    REQUIRE(put(initial,surface,pixels,4),"fixture pixels");
    if(id!=1u)REQUIRE(put(initial,0x500004u,other,4),"second surface slot");
    REQUIRE(put(initial,other,DATA+0x2801u+align,4)&&put(initial,other+12u,8,4)&&
        put(initial,other+16u,8,4),"second surface");
}
static int store(uint32_t address,uint32_t value,uint32_t bytes)
{
    if(!put(expected,address,value,bytes))return 0;
    ++stores[format];
    if(address-ARGUMENTS<24u)++caller_stores;
    if(address-DATA<0x1000u||address-0x500000u<4096u)++metadata_stores;
    return 1;
}
static int copy_row(uint32_t dest,uint32_t source,uint32_t count)
{
    ++copies[accelerated];
    for(uint32_t offset=0;offset<count;){
        uint32_t n=accelerated&&count-offset>=4u?4u:1u,tmp[4];
        for(uint32_t j=0;j<n;++j){
            uint32_t a=source+(offset+j)*4u;
            if(!valid(a,4))return 0;
            tmp[j]=get32(expected,a);
        }
        for(uint32_t j=0;j<n;++j)if(!store(dest+(offset+j)*4u,tmp[j],4))return 0;
        offset+=n;
    }
    return 1;
}
static int oracle_body(uint32_t *result)
{
    uint32_t xmin,xmax,ymin,ymax;geometry(&xmin,&xmax,&ymin,&ymax);
    uint32_t span=(uint32_t)((uint64_t)xmax-xmin+1u);
    if(!format){
        uint32_t packed=args[5]|args[5]<<8|args[5]<<16|args[5]<<24;
        int64_t blocks=signed32(span)/4,tail=signed32(span)%4;
        if(signed32(ymax)-signed32(ymin)>20||blocks>128)return 0;
        uint32_t y=ymin;
        while(signed32(y)<=signed32(ymax)){
            for(uint32_t i=0;i<(blocks>0?(uint32_t)blocks:0u)+(tail>0?(uint32_t)tail:0u);++i){
                uint32_t id=get32(expected,ARGUMENTS),slot=(uint32_t)(0x500000ULL+(uint64_t)id*4u);
                if(!valid(slot,4))return 0;
                uint32_t surface=get32(expected,slot);
                if(!valid(surface,4)||!valid(surface+12u,4))return 0;
                uint32_t block=i<(blocks>0?(uint32_t)blocks:0u);
                uint32_t offset=block?i*4u:(uint32_t)(blocks>0?blocks:0)*4u+i-(uint32_t)(blocks>0?blocks:0);
                uint64_t row=(uint64_t)y*get32(expected,surface+12u);
                uint32_t address=(uint32_t)((uint64_t)get32(expected,surface)+row+xmin+offset);
                uint32_t value=block?packed:get32(expected,ARGUMENTS+20u);
                if(!store(address,value,block?4u:1u))return 0;
                if(block)++block_stores;else ++tail_stores;
            }
            y=(uint32_t)((uint64_t)y+1u);
            if(y==0x80000000u)return 0; /* would wrap to another large row range */
        }
        *result=y;return 1;
    }
    uint32_t rows=(uint32_t)((uint64_t)ymax-ymin);
    if(span>512u||rows>20u)return 0;
    uint32_t surface=get32(expected,0x500000u+args[0]*4u);
    uint32_t pitch=(uint32_t)((uint64_t)get32(expected,surface+12u)*4u);
    uint32_t first=(uint32_t)((uint64_t)get32(expected,surface)+(uint64_t)ymin*pitch+(uint64_t)xmin*4u);
    for(uint32_t i=0;i<span;++i)
        if(!store(first+i*4u,get32(expected,ARGUMENTS+20u),4))return 0;
    uint32_t next=first;
    for(uint32_t row=0;row<rows;++row){
        next=(uint32_t)((uint64_t)next+pitch);
        if(!copy_row(next,first,span))return 0;
    }
    *result=ARGUMENTS-48u;return 1;
}
static int oracle(uint32_t *result)
{
    uint32_t *counters[]={&stores[0],&stores[1],&copies[0],&copies[1],&block_stores,
        &tail_stores,&metadata_stores,&caller_stores,&negative_clips,&clamps};
    uint32_t saved[10];
    for(uint32_t i=0;i<10;++i)saved[i]=*counters[i];
    if(oracle_body(result))return 1;
    for(uint32_t i=0;i<10;++i)*counters[i]=saved[i];
    return 0;
}
static void services(uint32_t base)
{
    *(volatile uint32_t *)0x1b0e4=SERVICES;
    for(uint32_t i=0;i<4096;++i)*(volatile uint8_t *)(SERVICES+i)=(uint8_t)(i*37u+cases);
    *(volatile uint32_t *)(SERVICES+68u)=base+(accelerated?0x173dau:0x17322u);
}
static void run(void)
{
    scenario=cases+skipped;begin_oracle();uint32_t value;
    if(!oracle(&value)){++skipped;return;}
    const void *functions[]={format?(void *)0x91bd26:(void *)0x91ba8a,
        format?(void *)kernel_raster_fill32_81bd26:(void *)kernel_raster_fill8_81ba8a,
        format?(void *)0x81bd26:(void *)0x81ba8a};
    for(implementation=0;implementation<3;++implementation){
        reset();services(implementation?0x800000u:0x900000u);uint32_t actual[7];
        for(uint32_t i=0;i<4096;++i)direct_saved_services[i]=*(volatile uint8_t *)(SERVICES+i);
        REQUIRE(!raster_checked_call(functions[implementation],args,actual),"fill ABI mismatch");
        compare(actual,value);
        for(uint32_t i=0;i<4096;++i)
            REQUIRE(direct_saved_services[i]==*(volatile uint8_t *)(SERVICES+i),"dispatch table changed");
        REQUIRE(*(volatile uint32_t *)0x1b0e4==SERVICES,"service table pointer changed");
    }
    ++cases;++counts[format];
}
void _start(void)
{
    mappings();REQUIRE(map(0x1b000,4096)&&map(SERVICES,4096),"copy service mappings");
    REQUIRE(load("build/raster_fill_original.x",0x900000)&&
        load("build/raster_fill_candidate.x",0x800000),"fill image load");
    static const uint32_t coordinates[]={0,1,2,3,6,7,8,9,0xfffffffe,0xffffffff,0x7fffffff,0x80000000};
    stage=1;
    for(format=0;format<2;++format)for(accelerated=0;accelerated<(format?2u:1u);++accelerated)
      for(align=0;align<4;++align)for(uint32_t x=0;x<12;++x)for(uint32_t y=0;y<12;++y)
      for(uint32_t reverse=0;reverse<4;++reverse){
        setup(ids[(x+y)%16u],8,8,reverse&1u?coordinates[x]:2u,reverse&2u?coordinates[y]:3u,
            reverse&1u?2u:coordinates[x],reverse&2u?3u:coordinates[y],DATA+0x2801u+align,random32());run();
      }
    stage=2;
    for(format=0;format<2;++format)for(accelerated=0;accelerated<(format?2u:1u);++accelerated)
      for(align=0;align<4;++align)for(uint32_t w=0;w<16;++w)for(uint32_t h=0;h<16;++h)
      for(uint32_t p=0;p<4;++p){
        setup(ids[(w+h+p)%16u],values[w],values[h],p?0xfffffffeu:values[w],p&1u?1u:values[h],
            p&2u?6u:0u,p&2u?3u:0u,DATA+0x2801u+align,random32());run();
      }
    stage=3;
    for(format=0;format<2;++format)for(accelerated=0;accelerated<(format?2u:1u);++accelerated)
      for(align=0;align<4;++align)for(uint32_t c=0;c<256;++c)for(uint32_t span=1;span<=9;++span){
        setup(0,16,3,1,0,span,2,DATA+0x2801u+align,
              c|(c^255u)<<8|(c^0x96u)<<16|(c^0x69u)<<24);run();
      }
    stage=4;
    for(format=0;format<2;++format)for(accelerated=0;accelerated<(format?2u:1u);++accelerated)
      for(align=0;align<4;++align)for(uint32_t alias=0;alias<9;++alias){
        static const uint32_t offsets[]={0,4,8,12,16,40};
        uint32_t dest=alias<6u?DATA+0x101u+align+offsets[alias]:0x500000u+(alias-6u)*4u;
        uint32_t color=alias==0?DATA+0x2801u+align:alias>=6?DATA+0x201u+align:4u;
        setup(0,8,3,0,0,6,2,dest,color);run();
      }
    for(format=0;format<2;++format)for(accelerated=0;accelerated<(format?2u:1u);++accelerated)
      for(align=0;align<4;++align)for(uint32_t offset=0;offset<24;++offset){
        setup(0,8,1,0,0,3,0,ARGUMENTS+offset,1);run();
      }
    stage=5;
    static const uint32_t spans[]={1,2,3,4,5,7,15,16,17,31,63,299,300,301,304,511};
    for(format=0;format<2;++format)for(accelerated=0;accelerated<(format?2u:1u);++accelerated)
      for(align=0;align<4;++align)for(uint32_t s=0;s<16;++s){
        setup(0,512,3,0,0,spans[s]-1u,2,DATA+0x2001u+align,random32());run();
      }
    /* Wrapped pitch creates overlapping rows with independently selected
     * scalar and SSE dispatchers. */
    for(accelerated=0;accelerated<2;++accelerated)for(align=0;align<4;++align)
      for(uint32_t w=0;w<4;++w)for(uint32_t s=0;s<11;++s){
        format=1;setup(0,0x40000000u+w,4,0,0,spans[s]-1u,3,DATA+0x2001u+align,random32());run();
      }
    /* Unaligned first-row stores change the live color word, producing
     * nonuniform source pixels before the overlapping copy executes. */
    stage=6;
    for(accelerated=0;accelerated<2;++accelerated)for(align=0;align<4;++align)
      for(uint32_t offset=0;offset<24;++offset)for(uint32_t span=1;span<=5;++span){
        format=1;setup(0,0x40000001u,2,0,0,span-1u,1,ARGUMENTS+offset,0x12345678u);run();
      }
    for(format=0;format<2;++format){
        accelerated=0;align=0;setup(0,8,3,0,0,6,2,DATA+0x2801u,0x12345678u);
        uint32_t surface=DATA+16384u-20u;
        REQUIRE(put(initial,0x500000u,surface,4)&&put(initial,surface,DATA+0x2801u,4)&&
            put(initial,surface+12u,8,4)&&put(initial,surface+16u,3,4),"guarded fill surface");run();
    }
    PRINT("Fill cases8/32: ");number(counts[0]);PRINT(" ");number(counts[1]);
    PRINT("; executions: ");number(cases*3u);PRINT("; excluded large/unmapped scenarios: ");number(skipped);
    PRINT("; stores8/32: ");number(stores[0]);PRINT(" ");number(stores[1]);
    PRINT("; block/tail stores: ");number(block_stores);PRINT(" ");number(tail_stores);
    PRINT("; scalar/SSE copy rows: ");number(copies[0]);PRINT(" ");number(copies[1]);
    PRINT("; metadata/caller stores: ");number(metadata_stores);PRINT(" ");number(caller_stores);
    PRINT("; negative clipped coordinates: ");number(negative_clips);PRINT("; clamp decisions: ");number(clamps);
    PRINT("; rectangle fills verified against 32 KiB memory/return/live-argument/ABI oracle; real copy services; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
