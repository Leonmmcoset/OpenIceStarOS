/* Reuse only the byte-address/ABI fixture and previously verified leaf
 * fill/copy models. The composition model snapshots each callee's arguments
 * separately and reads the outer incoming words again after every call. */
#define _start rounded_previous_brush_start
#include "kernel_raster_brush.c"
#undef _start
#include "../kernel/raster_rounded.h"
static const uint32_t rounded_entries[]={0x81d6c2,0x81d8ca,0x81da68};
static uint32_t rounded_counts[3],primitive_calls[3],radius_changes,corner_tests,
    inside_tests,outside_tests,iterations;
#define A(i) get32(expected,ARGUMENTS+(i)*4u)
static int rounded_point(uint32_t id,uint32_t x,uint32_t y,uint32_t color,int clipped)
{
    uint32_t surface;
    if(clipped&&signed32(x)<0){++rejected_points;return 1;}
    if(!record(id,&surface))return 0;
    if(clipped&&(signed32(x)>=signed32(get32(expected,surface+12u))||signed32(y)<0||
       signed32(y)>=signed32(get32(expected,surface+16u)))){++rejected_points;return 1;}
    uint32_t address=(uint32_t)((uint64_t)get32(expected,surface)+
        ((uint64_t)y*get32(expected,surface+12u)+x)*(format?4u:1u));
    return store(address,color,format?4u:1u);
}
static int rounded_line(uint32_t id,uint32_t x0,uint32_t y0,uint32_t x1,
    uint32_t y1,uint32_t color,uint32_t *result)
{
    ++primitive_calls[0];
    uint32_t ends[4]={x0,y0,x1,y1},magnitude[2],step[2];
    for(unsigned a=0;a<2;++a){
        uint32_t diff=ends[a+2u]-ends[a];magnitude[a]=signed32(diff)<0?0u-diff:diff;
    }
    uint32_t major=signed32(magnitude[0])>=signed32(magnitude[1])?0u:1u,minor=major^1u;
    uint32_t count=magnitude[major]+1u;
    int64_t adjust=signed32(ends[minor])<=signed32(ends[minor+2u])?1:-1;
    uint32_t numerator=(uint32_t)((uint64_t)(ends[minor+2u]-ends[minor]+adjust)*1024u);
    if(!count||(numerator==0x80000000u&&count==UINT32_MAX)||signed32(count)>64)return 0;
    step[minor]=quotient(numerator,count);
    step[major]=signed32(ends[major])>signed32(ends[major+2u])?0xfffffc00u:1024u;
    ++branch_counts[major];
    uint32_t position[2]={x0<<10,y0<<10},i=0;
    for(;signed32(i)<signed32(count);++i){
        if(!rounded_point(id,fixed_integer(position[0]),fixed_integer(position[1]),color,0))return 0;
        for(unsigned a=0;a<2;++a)position[a]+=step[a];
    }
    *result=i;return 1;
}
static int rounded_quadrant(uint32_t id,uint32_t cx,uint32_t cy,uint32_t radius,
    unsigned quadrant,uint32_t color)
{
    ++primitive_calls[1];
    if(signed32(radius)>24)return 0;
    uint32_t x=0,y=radius,error=3u-radius*2u;
    while(signed32(x)<=signed32(y)){
        for(unsigned pair=0;pair<2;++pair){
            uint32_t dx=pair?y:x,dy=pair?x:y;
            if(!rounded_point(id,quadrant&1u?cx-dx:cx+dx,
                quadrant&2u?cy-dy:cy+dy,color,1))return 0;
        }
        if(signed32(error)<0)error+=(x<<2)+6u;
        else{error+=((x-y)<<2)+10u;--y;}
        ++x;
    }
    return 1;
}
static int rounded_fill_call(uint32_t id,uint32_t x0,uint32_t y0,uint32_t x1,
    uint32_t y1,uint32_t color,uint32_t *result)
{
    ++primitive_calls[2];
    if(!fill(id,x0,y0,x1,y1,color,result))return 0;
    /* Outer EBP = ARGUMENTS-8; original outgoing base = EBP-40.
     * The forwarder and 32-bit fill leave EAX at ARGUMENTS-144. */
    if(format)*result=ARGUMENTS-144u;
    return 1;
}
static int rounded_oracle_body(uint32_t *result)
{
    if(!kind){
        uint32_t cx[4]={A(1)+A(5),A(1)+A(3)-A(5)-1u,A(1)+A(5),A(1)+A(3)-A(5)-1u};
        uint32_t cy[4]={A(2)+A(5),A(2)+A(5),A(2)+A(4)-A(5)-1u,A(2)+A(4)-A(5)-1u};
        static const unsigned quadrant[]={3,2,1,0};
        for(unsigned q=0;q<4;++q){
            if(!rounded_quadrant(A(0),cx[q],cy[q],A(5),quadrant[q],A(6)))return 0;
        }
        if(!rounded_line(A(0),A(1)+A(5),A(2),A(1)+A(3)-A(5),A(2),A(6),result))return 0;
        if(!rounded_line(A(0),A(1)+A(5),A(2)+A(4)-1u,A(1)+A(3)-A(5),A(2)+A(4)-1u,A(6),result))return 0;
        if(!rounded_line(A(0),A(1),A(2)+A(5),A(1),A(2)+A(4)-A(5),A(6),result))return 0;
        return rounded_line(A(0),A(1)+A(3)-1u,A(2)+A(5),A(1)+A(3)-1u,A(2)+A(4)-A(5),A(6),result);
    }
    if(kind==1){
        uint32_t i=0;
        for(;signed32(i)<signed32(A(5)*2u);++i){
            if(++iterations>2048)return 0;
            for(uint32_t j=0;signed32(j)<signed32(A(5)*2u);++j){
                if(++iterations>2048)return 0;
                uint32_t dx=A(5)-i,dy=A(5)-j;
                uint32_t distance=(uint32_t)((uint64_t)dx*dx+(uint64_t)dy*dy);
                uint32_t square=(uint32_t)((uint64_t)A(5)*A(5));
                ++corner_tests;
                if(signed32(distance)>signed32(square)){++outside_tests;continue;}
                ++inside_tests;
                for(unsigned p=0;p<4;++p){
                    uint32_t xx=p&1u?A(1)+A(3)-i-1u:A(1)+i;
                    uint32_t yy=p&2u?A(2)+A(4)-j-1u:A(2)+j;
                    uint32_t old_radius=A(5);
                    if(!rounded_point(A(0),xx,yy,A(6),1))return 0;
                    if(old_radius!=A(5))++radius_changes;
                }
            }
        }
        if(!rounded_fill_call(A(0),A(1)+A(5),A(2),A(1)+A(3)-A(5)-1u,A(2)+A(4)-1u,A(6),result))return 0;
    }else{
        for(uint32_t i=0;signed32(i)<signed32(A(5));++i){
            if(++iterations>48)return 0;
            uint32_t old_radius=A(5);
            if(!rounded_line(A(0),A(1)+A(5)-i,A(2)+i,A(1)+A(3)-A(5)+i-1u,A(2)+i,A(6),result))return 0;
            if(old_radius!=A(5))++radius_changes;
            old_radius=A(5);
            if(!rounded_line(A(0),A(1)+A(5)-i,A(2)+A(4)-i-1u,A(1)+A(3)-A(5)+i-1u,A(2)+A(4)-i-1u,A(6),result))return 0;
            if(old_radius!=A(5))++radius_changes;
        }
    }
    return rounded_fill_call(A(0),A(1),A(2)+A(5),A(1)+A(3)-1u,A(2)+A(4)-A(5)-1u,A(6),result);
}
static int rounded_oracle(uint32_t *result)
{
    uint32_t *counters[]={&stores[0],&stores[1],&stores[2],&metadata_stores,&caller_stores,
        &branch_counts[0],&branch_counts[1],&rejected_points,&block_stores,&row_copies,
        &primitive_calls[0],&primitive_calls[1],&primitive_calls[2],&radius_changes,
        &corner_tests,&inside_tests,&outside_tests},saved[17];
    for(unsigned i=0;i<17;++i)saved[i]=*counters[i];
    iterations=0;
    if(rounded_oracle_body(result))return 1;
    for(unsigned i=0;i<17;++i)*counters[i]=saved[i];
    return 0;
}
static void rounded_services(uint32_t base)
{
    services(base);
    *(volatile uint32_t *)(SERVICES+4)=base+(format?0x1b8dcu:0x1b734u);
}
static void rounded_run(void)
{
    scenario=cases+skipped;begin_oracle();uint32_t value;
    if(!rounded_oracle(&value)){++skipped;return;}
    const void *native[]={(void *)kernel_rounded_outline_81d6c2,
        (void *)kernel_rounded_fill_81d8ca,(void *)kernel_chamfer_fill_81da68};
    const void *functions[]={(void *)(uintptr_t)(rounded_entries[kind]+0x100000u),
        native[kind],(void *)(uintptr_t)rounded_entries[kind]};
    for(implementation=0;implementation<3;++implementation){
        reset();rounded_services(implementation?0x800000u:0x900000u);uint32_t actual[9];
        for(unsigned i=0;i<1280;++i)saved_services[i]=*(volatile uint8_t *)(SERVICES+i);
        REQUIRE(!raster_checked_call(functions[implementation],args,actual),"rounded ABI");
        compare(actual,value);
        for(unsigned i=0;i<1280;++i)REQUIRE(saved_services[i]==*(volatile uint8_t *)(SERVICES+i),"rounded service table");
        REQUIRE(*(volatile uint32_t *)0x1b0e4==SERVICES,"rounded globals");
    }
    for(unsigned i=0;i<8;++i)if(A(i)!=args[i])++changed_words;
    ++rounded_counts[kind];++cases;
}
#ifdef ROUNDED_CHAIN
void rounded_direct_start(void)
#else
void _start(void)
#endif
{
    mappings();REQUIRE(map(0x1b000,4096)&&map(SERVICES,4096),"rounded maps");
    REQUIRE(load("build/raster_rounded_original.x",0x900000)&&load("build/raster_rounded_candidate.x",0x800000),"rounded images");
    stage=1;
    static const uint32_t radii[]={0,1,2,3,5,8,0xffffffffu,0x80000000u,0x80000001u};
    static const uint32_t lengths[]={0,1,3,8,16,0xffffffffu,0x80000000u};
    for(kind=0;kind<3;++kind)for(format=0;format<2;++format)
      for(accelerated=0;accelerated<(format?2u:1u);++accelerated)
      for(alignment=0;alignment<4;++alignment)for(unsigned r=0;r<9;++r)
      for(unsigned w=0;w<7;++w)for(unsigned h=0;h<7;++h){
        args[0]=ids[(r+w+h)%16u];args[1]=3;args[2]=4;
        args[3]=lengths[w];args[4]=lengths[h];args[5]=radii[r];args[6]=random32();
        setup(24,24,DATA+0x1001u+alignment);rounded_run();
    }
    stage=2;
    for(kind=0;kind<3;++kind)for(format=0;format<2;++format)
      for(alignment=0;alignment<4;++alignment)for(unsigned dim=0;dim<16;++dim)
      for(unsigned origin=0;origin<3;++origin)for(unsigned r=0;r<4;++r){
        accelerated=dim&1u;args[0]=ids[(dim+origin+r)%16u];
        args[1]=origin*5u-2u;args[2]=origin*4u-1u;args[3]=8;args[4]=7;args[5]=r;args[6]=random32();
        setup(values[dim],values[(dim+3u)%16u],DATA+0x1001u+alignment);rounded_run();
    }
    stage=3;
    for(kind=0;kind<3;++kind)for(format=0;format<2;++format)
      for(alignment=0;alignment<4;++alignment)for(unsigned alias=0;alias<9;++alias){
        accelerated=alias&1u;uint32_t surface=DATA+0x101u+alignment;
        uint32_t dest=alias<6u?surface+alias*4u:0x500000u+(alias-6u)*4u;
        args[0]=0;args[1]=0;args[2]=0;args[3]=3;args[4]=3;args[5]=1;
        args[6]=alias==0?DATA+0x2801u+alignment:alias>=6?DATA+0x201u+alignment:4u;
        setup(3,3,dest);rounded_run();
    }
    stage=4;
    for(kind=0;kind<3;++kind)for(format=0;format<2;++format)
      for(alignment=0;alignment<4;++alignment)for(unsigned at=0;at<29;++at){
        accelerated=at&1u;args[0]=0;args[1]=0;args[2]=0;
        args[3]=1;args[4]=1;args[5]=kind==0?0u:1u;args[6]=1;
        setup(kind?0u:1u,1,ARGUMENTS+at);rounded_run();
    }
    stage=5;
    /* Force a corner store / first chamfer edge to rewrite live radius.
     * The pixel destination is shifted so that the first accepted point
     * lands exactly on the outer sixth word, with valid surface dimensions. */
    for(kind=1;kind<3;++kind)for(format=0;format<2;++format)
      for(alignment=0;alignment<4;++alignment)for(unsigned radius=1;radius<4;++radius){
        accelerated=radius&1u;args[0]=0;args[1]=0;args[2]=0;
        args[3]=4;args[4]=1;args[5]=radius;args[6]=0;
        uint32_t first_pixel=kind==1?4u*radius:radius;
        setup(4,8,ARGUMENTS+20u-first_pixel*(format?4u:1u));rounded_run();
    }
    REQUIRE(caller_stores&&metadata_stores&&changed_words&&inside_tests&&outside_tests&&
        primitive_calls[0]&&primitive_calls[1]&&primitive_calls[2]&&block_stores&&row_copies&&radius_changes,"rounded coverage");
    PRINT("Rounded scenarios outline/fill/chamfer:");
    for(unsigned i=0;i<3;++i){PRINT(" ");number(rounded_counts[i]);}
    PRINT("; executions: ");number(cases*3u);PRINT("; excluded unsafe/large: ");number(skipped);
    PRINT("; metadata/caller stores/changed words: ");number(metadata_stores);PRINT(" ");number(caller_stores);PRINT(" ");number(changed_words);
    PRINT("; line/quadrant/fill calls:");for(unsigned i=0;i<3;++i){PRINT(" ");number(primitive_calls[i]);}
    PRINT("; corner tests/inside/outside/radius changes: ");number(corner_tests);PRINT(" ");number(inside_tests);PRINT(" ");number(outside_tests);PRINT(" ");number(radius_changes);
    PRINT("; blocks/row copies: ");number(block_stores);PRINT(" ");number(row_copies);
    PRINT("; rounded rectangles verified against 32 KiB memory/eight caller words/raw EAX/ABI oracle; real quadrant/line/fill/pixel/copy callees, no CALL substitutions\n");
    syscall3(1,0,0,0);
}
