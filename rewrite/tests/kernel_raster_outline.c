/* Independent signed, widened border oracle. Bytewise stores update live
 * caller words and surface metadata before the next store is planned. */
#include "../kernel/raster_outline.h"
#include "raster_fixture.h"
static uint32_t format,align,counts[2],stores[2],metadata_stores,caller_stores,
    changed_left,negative_coordinates,repeated_edges;
static int valid(uint32_t a,uint32_t bytes)
{for(uint32_t i=0;i<bytes;++i)if(index_of(a+i)==UINT32_MAX)return 0;return 1;}
static uint32_t lower(uint32_t a,uint32_t b){return signed32(a)<signed32(b)?a:b;}
static uint32_t upper(uint32_t a,uint32_t b){return signed32(a)>signed32(b)?a:b;}
static void setup(uint32_t id,uint32_t width,uint32_t x0,uint32_t y0,
                  uint32_t x1,uint32_t y1,uint32_t destination,uint32_t color)
{
    pattern();args[0]=id;args[1]=x0;args[2]=y0;args[3]=x1;args[4]=y1;args[5]=color;
    uint32_t surface=DATA+0x101u+align,other=DATA+0x201u+align;
    uint64_t index=(uint64_t)lower(y0,y1)*width+lower(x0,x1);
    REQUIRE(put(initial,0x500000u+id*4u,surface,4)&&
        put(initial,surface,destination-(uint32_t)(index*(format?4u:1u)),4)&&
        put(initial,surface+12u,width,4)&&put(initial,surface+16u,values[cases%16u],4),"outline surface");
    if(id!=1u)REQUIRE(put(initial,0x500004u,other,4),"second surface slot");
    REQUIRE(put(initial,other,DATA+0x2801u+align,4)&&put(initial,other+12u,8,4),"second surface");
}
static int point(uint32_t x,uint32_t y)
{
    uint32_t id=get32(expected,ARGUMENTS),slot=(uint32_t)(0x500000ULL+(uint64_t)id*4u);
    if(!valid(slot,4))return 0;
    uint32_t surface=get32(expected,slot);
    if(!valid(surface,4)||!valid(surface+12u,4))return 0;
    uint64_t index=(uint64_t)y*get32(expected,surface+12u)+x;
    uint32_t address=(uint32_t)((uint64_t)get32(expected,surface)+index*(format?4u:1u));
    if(!put(expected,address,get32(expected,ARGUMENTS+20u),format?4u:1u))return 0;
    ++stores[format];
    if(address-ARGUMENTS<24u)++caller_stores;
    if(address-DATA<0x1000u||address-0x500000u<4096u)++metadata_stores;
    if(signed32(x)<0||signed32(y)<0)++negative_coordinates;
    return 1;
}
static int oracle_body(uint32_t *result)
{
    uint32_t xmin=lower(args[1],args[3]),xmax=upper(args[1],args[3]);
    uint32_t ymin=lower(args[2],args[4]),ymax=upper(args[2],args[4]);
    if(signed32(xmax)-signed32(xmin)>32||signed32(ymax)-signed32(ymin)>32||
        xmax==0x7fffffffu||ymax==0x7fffffffu)return 0;
    if(xmin==xmax||ymin==ymax)++repeated_edges;
    for(int64_t x=signed32(xmin);x<=signed32(xmax);++x)
        if(!point((uint32_t)x,ymin)||!point((uint32_t)x,ymax))return 0;
    uint32_t left=lower(get32(expected,ARGUMENTS+4u),get32(expected,ARGUMENTS+12u));
    if(left!=xmin)++changed_left;
    for(int64_t y=signed32(ymin);y<=signed32(ymax);++y)
        if(!point(left,(uint32_t)y)||!point(xmax,(uint32_t)y))return 0;
    *result=(uint32_t)((uint64_t)ymax+1u);return 1;
}
static int oracle(uint32_t *result)
{
    uint32_t *counters[]={&stores[0],&stores[1],&metadata_stores,&caller_stores,
        &changed_left,&negative_coordinates,&repeated_edges},saved[7];
    for(uint32_t i=0;i<7;++i)saved[i]=*counters[i];
    if(oracle_body(result))return 1;
    for(uint32_t i=0;i<7;++i)*counters[i]=saved[i];
    return 0;
}
static void run(void)
{
    scenario=cases+skipped;begin_oracle();uint32_t value;
    if(!oracle(&value)){++skipped;return;}
    const void *functions[]={format?(void *)0x91c1e4:(void *)0x91c058,
        format?(void *)kernel_raster_outline32_81c1e4:(void *)kernel_raster_outline8_81c058,
        format?(void *)0x81c1e4:(void *)0x81c058};
    for(implementation=0;implementation<3;++implementation){
        reset();uint32_t actual[7];
        REQUIRE(!raster_checked_call(functions[implementation],args,actual),"outline ABI mismatch");
        compare(actual,value);
    }
    ++cases;++counts[format];
}
void _start(void)
{
    mappings();REQUIRE(load("build/raster_outline_original.x",0x900000)&&
        load("build/raster_outline_candidate.x",0x800000),"outline image load");
    static const uint32_t origins[][2]={{0,0},{4,3},{0xfffffffc,0xfffffffe},
        {0x40000001,0x20000000},{0x7ffffff0,0x80000000}};
    stage=1;
    for(format=0;format<2;++format)for(align=0;align<4;++align)for(uint32_t o=0;o<5;++o)
      for(uint32_t dx=0;dx<13;++dx)for(uint32_t dy=0;dy<13;++dy){
        uint32_t x=origins[o][0],y=origins[o][1];
        setup(ids[(o+dx+dy)%16u],31,x,y,x+dx-6u,y+dy-6u,DATA+0x2801u+align,random32());run();
      }
    stage=2;
    for(format=0;format<2;++format)for(align=0;align<4;++align)for(uint32_t w=0;w<16;++w)
      for(uint32_t x=0;x<16;++x)for(uint32_t y=0;y<16;++y){
        setup(ids[(w+x+y)%16u],values[w],values[x],values[y],values[x],values[y],DATA+0x2801u+align,random32());run();
      }
    stage=3;
    for(format=0;format<2;++format)for(align=0;align<4;++align)for(uint32_t c=0;c<256;++c)
      for(uint32_t shape=0;shape<4;++shape){
        setup(0,31,shape&1u?4u:1u,shape&2u?4u:1u,4,4,DATA+0x2801u+align,
            c|(c^255u)<<8|(c^0x96u)<<16|(c^0x69u)<<24);run();
      }
    stage=4;
    for(format=0;format<2;++format)for(align=0;align<4;++align)for(uint32_t a=0;a<9;++a){
        static const uint32_t offsets[]={0,4,8,12,16,40};
        uint32_t dest=a<6u?DATA+0x101u+align+offsets[a]:0x500000u+(a-6u)*4u;
        uint32_t color=a==0?DATA+0x2801u+align:a>=6?DATA+0x201u+align:4u;
        setup(0,3,0,0,2,2,dest,color);run();
      }
    for(format=0;format<2;++format)for(align=0;align<4;++align)for(uint32_t offset=0;offset<24;++offset)
      for(uint32_t reverse=0;reverse<2;++reverse){
        setup(0,0,reverse?1u:0u,0,reverse?0u:1u,0,ARGUMENTS+offset,0);run();
      }
    /* The horizontal pass raises the smaller input X to one. Only the left
     * edge is recaptured; the right edge and all Y geometry stay captured. */
    for(format=0;format<2;++format)for(align=0;align<4;++align)for(uint32_t reverse=0;reverse<2;++reverse){
        setup(0,0,reverse?1u:0u,0,reverse?0u:1u,0,
            ARGUMENTS+(reverse?12u:4u),1);run();
    }
    /* Unaligned DWORD stores straddle Y1/color, changing the next live color. */
    for(format=0;format<2;++format)for(align=0;align<4;++align)for(uint32_t offset=16;offset<=20;++offset){
        setup(0,0,0,0,0,0,ARGUMENTS+offset,0x12345678u);run();
    }
    stage=5;
    for(format=0;format<2;++format)for(uint32_t x=0;x<16;++x)for(uint32_t y=0;y<16;++y){
        setup(0,7,0,0,values[x],values[y],DATA+0x2801u,random32());run();
      }
    REQUIRE(changed_left>0&&caller_stores>0&&metadata_stores>0&&negative_coordinates>0,"missing alias coverage");
    for(format=0;format<2;++format){
        align=0;setup(0,8,0,0,2,2,DATA+0x2801u,0x12345678u);
        uint32_t surface=DATA+16384u-16u;
        REQUIRE(put(initial,0x500000u,surface,4)&&put(initial,surface,DATA+0x2801u,4)&&
            put(initial,surface+12u,8,4),"guarded outline surface");run();
      }
    PRINT("Outline cases8/32: ");number(counts[0]);PRINT(" ");number(counts[1]);
    PRINT("; executions: ");number(cases*3u);PRINT("; excluded large/unmapped scenarios: ");number(skipped);
    PRINT("; stores8/32: ");number(stores[0]);PRINT(" ");number(stores[1]);
    PRINT("; metadata/caller stores: ");number(metadata_stores);PRINT(" ");number(caller_stores);
    PRINT("; changed left edges: ");number(changed_left);PRINT("; negative-coordinate stores: ");number(negative_coordinates);
    PRINT("; degenerate-edge scenarios: ");number(repeated_edges);
    PRINT("; rectangle outlines verified against 32 KiB memory/return/live-argument/ABI oracle; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
