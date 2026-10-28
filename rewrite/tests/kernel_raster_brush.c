/* Independent widened arithmetic / byte-address oracle. All callees execute
 * real original or recovered code, including selected pixel/fill/copy slots.
 * Output may overwrite outer caller words, descriptors and ID table entries.
 */
#include "../kernel/raster_brush.h"
#include "raster_fixture.h"
#define SERVICES 0x620000u
#define INCOMING_EAX 0x6e25c813u
static uint32_t kind,format,alignment,accelerated,counts[5],stores[5],
    metadata_stores,caller_stores,changed_words,branch_counts[2],zero_circles,
    rejected_points,block_stores,row_copies;
static uint32_t palette_base;
static uint8_t saved_services[1280];
static const uint32_t entries[]={0x81b714,0x81d032,0x81d070,0x81d1fc,0x81d4e6};
static int valid(uint32_t a,uint32_t n)
{for(uint32_t i=0;i<n;++i)if(index_of(a+i)==UINT32_MAX)return 0;return 1;}
static uint32_t low(uint32_t a,uint32_t b){return signed32(a)<signed32(b)?a:b;}
static uint32_t high(uint32_t a,uint32_t b){return signed32(a)>signed32(b)?a:b;}
static int record(uint32_t id,uint32_t *surface)
{
    uint32_t slot=(uint32_t)(0x500000ULL+(uint64_t)id*4u);
    if(!valid(slot,4))return 0;
    *surface=get32(expected,slot);
    return valid(*surface,4)&&valid(*surface+12u,8);
}
static int store(uint32_t address,uint32_t color,uint32_t bytes)
{
    if(!put(expected,address,color,bytes))return 0;
    ++stores[kind];
    if(address-ARGUMENTS<32u)++caller_stores;
    if(address-DATA<0x1000u||address-0x500000u<4096u)++metadata_stores;
    return 1;
}
static uint32_t clip(uint32_t coordinate,uint32_t dimension)
{
    int64_t v=signed32(coordinate),bound=signed32(dimension-1u);
    if(v<0)v=0;
    if(v>bound)v=bound;
    return (uint32_t)v;
}
static int fill(uint32_t id,uint32_t x0,uint32_t y0,uint32_t x1,uint32_t y1,
    uint32_t color,uint32_t *result)
{
    uint32_t surface;if(!record(id,&surface))return 0;
    uint32_t width=get32(expected,surface+12u),height=get32(expected,surface+16u);
    uint32_t xmin=clip(low(x0,x1),width),xmax=clip(high(x0,x1),width);
    uint32_t ymin=clip(low(y0,y1),height),ymax=clip(high(y0,y1),height);
    uint32_t span=xmax-xmin+1u,rows=ymax-ymin;
    if(signed32(ymax)-signed32(ymin)>20||ymax==0x7fffffffu)return 0;
    if(!format){
        int64_t blocks=signed32(span)/4,tail=signed32(span)%4;
        if(blocks>32)return 0;
        uint32_t packed=color|color<<8|color<<16|color<<24;
        for(int64_t y=signed32(ymin);y<=signed32(ymax);++y){
            for(int64_t i=0;i<blocks;++i){
                if(!record(id,&surface))return 0;
                uint32_t address=(uint32_t)((uint64_t)get32(expected,surface)+
                    (uint64_t)(uint32_t)y*get32(expected,surface+12u)+xmin+(uint32_t)i*4u);
                if(!store(address,packed,4))return 0;
                ++block_stores;
            }
            for(int64_t i=0;i<tail;++i){
                if(!record(id,&surface))return 0;
                uint32_t offset=(uint32_t)(blocks>0?blocks:0)*4u+(uint32_t)i;
                uint32_t address=(uint32_t)((uint64_t)get32(expected,surface)+
                    (uint64_t)(uint32_t)y*get32(expected,surface+12u)+xmin+offset);
                if(!store(address,color,1))return 0;
            }
        }
        *result=ymax+1u;return 1;
    }
    if(span>128u||rows>20u)return 0;
    uint32_t pitch=width*4u,first=get32(expected,surface)+ymin*pitch+xmin*4u;
    for(uint32_t i=0;i<span;++i)if(!store(first+i*4u,color,4))return 0;
    for(uint32_t row=1;row<=rows;++row){
        ++row_copies;
        for(uint32_t i=0;i<span;){
            uint32_t n=accelerated&&span-i>=4u?4u:1u,values[4];
            for(uint32_t j=0;j<n;++j){
                uint32_t address=first+(i+j)*4u;
                if(!valid(address,4))return 0;
                values[j]=get32(expected,address);
            }
            for(uint32_t j=0;j<n;++j)if(!store(first+row*pitch+(i+j)*4u,values[j],4))return 0;
            i+=n;
        }
    }
    *result=ARGUMENTS-128u;return 1;
}
static int square(uint32_t id,uint32_t x,uint32_t y,uint32_t color,uint32_t side,uint32_t *result)
{return fill(id,x,y,x+side-1u,y+side-1u,color,result);}
static int live_square(uint32_t x,uint32_t y)
{
    uint32_t result,color_at=kind==2?20u:16u,side_at=kind==2?24u:20u;
    return square(get32(expected,ARGUMENTS),x,y,get32(expected,ARGUMENTS+color_at),
        get32(expected,ARGUMENTS+side_at),&result);
}
static uint32_t fixed_integer(uint32_t v)
{
    int64_t n=signed32(v),q=n/1024;
    if(n<0&&n%1024)--q;
    return (uint32_t)q;
}
static uint32_t quotient(uint32_t numerator,uint32_t denominator)
{
    int64_t sn=signed32(numerator),sd=signed32(denominator);
    uint64_t n=sn<0?-sn:sn,d=sd<0?-sd:sd,result=0;
    for(int bit=31;bit>=0;--bit)if((d<<bit)<=n){n-=d<<bit;result|=1ULL<<bit;}
    return (uint32_t)((sn<0)!=(sd<0)?0ULL-result:result);
}
static int line(uint32_t *result)
{
    uint32_t ends[4]={args[1],args[2],args[3],args[4]},magnitude[2],step[2];
    for(unsigned a=0;a<2;++a){
        uint32_t diff=ends[a+2u]-ends[a];
        magnitude[a]=signed32(diff)<0?0u-diff:diff;
    }
    uint32_t major=signed32(magnitude[0])>=signed32(magnitude[1])?0u:1u,minor=major^1u;
    uint32_t count=magnitude[major]+1u;
    int64_t adjust=signed32(ends[minor])<=signed32(ends[minor+2u])?1:-1;
    uint32_t numerator=(uint32_t)((uint64_t)(ends[minor+2u]-ends[minor]+adjust)*1024u);
    if(!count||(numerator==0x80000000u&&count==UINT32_MAX)||signed32(count)>48)return 0;
    step[minor]=quotient(numerator,count);
    step[major]=signed32(ends[major])>signed32(ends[major+2u])?0xfffffc00u:1024u;
    ++branch_counts[major];
    uint32_t position[2]={ends[0]<<10,ends[1]<<10},i=0;
    for(;signed32(i)<signed32(count);++i){
        if(!live_square(fixed_integer(position[0]),fixed_integer(position[1])))return 0;
        for(unsigned a=0;a<2;++a)position[a]+=step[a];
    }
    *result=i;return 1;
}
static int brush_circle(uint32_t *result)
{
    uint32_t radius=args[3];
    if(!radius){++zero_circles;*result=INCOMING_EAX;return 1;}
    if(signed32(radius)>24)return 0;
    uint32_t x=0,y=radius;
    int64_t half=signed32(radius)/2;
    if(signed32(radius)<0&&radius%2u)--half;
    uint32_t error=0u-(uint32_t)half;
    for(unsigned iteration=0;signed32(y)>=signed32(x);++iteration){
        if(iteration>=48)return 0;
        uint32_t cx=get32(expected,ARGUMENTS+4u),cy=get32(expected,ARGUMENTS+8u);
        const uint32_t points[8][2]={{cx+x-1u,cy+y-1u},{cx+y-1u,cy+x-1u},
            {cx-x,cy+y-1u},{cx-y,cy+x-1u},{cx-x,cy-y},{cx-y,cy-x},
            {cx+x-1u,cy-y},{cx+y-1u,cy-x}};
        for(unsigned p=0;p<8;++p){
            uint32_t xx=points[p][0],yy=points[p][1],surface;
            int positive_x=p==0||p==1||p>=6,positive_y=p<4;
            if(positive_x){
                if(!record(get32(expected,ARGUMENTS),&surface))return 0;
                if(signed32(xx)>=signed32(get32(expected,surface+12u)))continue;
            }else if(signed32(xx)<0)continue;
            if(positive_y){
                if(!record(get32(expected,ARGUMENTS),&surface))return 0;
                if(signed32(yy)>=signed32(get32(expected,surface+16u)))continue;
            }else if(signed32(yy)<0)continue;
            if(!live_square(xx,yy))return 0;
        }
        ++x;
        if(signed32(error)<0)error+=x;
        else{--y;error+=x-y;}
    }
    *result=y;return 1;
}
static int point(uint32_t x,uint32_t y)
{
    if(signed32(x)<0){++rejected_points;return 1;}
    uint32_t surface;if(!record(get32(expected,ARGUMENTS),&surface))return 0;
    if(signed32(x)>=signed32(get32(expected,surface+12u))||signed32(y)<0||
       signed32(y)>=signed32(get32(expected,surface+16u))){++rejected_points;return 1;}
    uint32_t address=(uint32_t)((uint64_t)get32(expected,surface)+
        ((uint64_t)y*get32(expected,surface+12u)+x)*(format?4u:1u));
    return store(address,get32(expected,ARGUMENTS+20u),format?4u:1u);
}
static int quadrants(uint32_t *result)
{
    uint32_t x=0,y=args[3],error=3u-y*2u;
    if(signed32(y)>24)return 0;
    for(unsigned iteration=0;signed32(x)<=signed32(y);++iteration){
        if(iteration>=48)return 0;
        for(unsigned bit=0;bit<4;++bit)if(get32(expected,ARGUMENTS+16u)&(1u<<bit)){
            for(unsigned pair=0;pair<2;++pair){
                uint32_t cx=get32(expected,ARGUMENTS+4u),cy=get32(expected,ARGUMENTS+8u);
                uint32_t dx=pair?y:x,dy=pair?x:y;
                if(!point(bit&1u?cx-dx:cx+dx,bit&2u?cy-dy:cy+dy))return 0;
            }
        }
        if(signed32(error)<0)error+=(x<<2)+6u;
        else{error+=((x-y)<<2)+10u;--y;}
        ++x;
    }
    *result=x;return 1;
}
static int oracle_body(uint32_t *result)
{
    switch(kind){
    case 0:{uint32_t address=palette_base+args[0]*4u;
        if(!valid(address,4))return 0;
        *result=get32(expected,address);return 1;}
    case 1:return square(args[0],args[1],args[2],args[3],args[4],result);
    case 2:return line(result);
    case 3:return brush_circle(result);
    default:return quadrants(result);
    }
}
static int oracle(uint32_t *result)
{
    uint32_t *counters[]={&stores[0],&stores[1],&stores[2],&stores[3],&stores[4],
        &metadata_stores,&caller_stores,&branch_counts[0],&branch_counts[1],
        &zero_circles,&rejected_points,&block_stores,&row_copies},saved[13];
    for(unsigned i=0;i<13;++i)saved[i]=*counters[i];
    if(oracle_body(result))return 1;
    for(unsigned i=0;i<13;++i)*counters[i]=saved[i];
    return 0;
}
static void setup(uint32_t width,uint32_t height,uint32_t destination)
{
    pattern();uint32_t surface=DATA+0x101u+alignment,other=DATA+0x201u+alignment;
    REQUIRE(put(initial,0x500000u+args[0]*4u,surface,4)&&put(initial,surface,destination,4)&&
        put(initial,surface+12u,width,4)&&put(initial,surface+16u,height,4),"brush fixture");
    if((args[0]&0x3fffffffu)!=1u)REQUIRE(put(initial,0x500004u,other,4),"brush other slot");
    REQUIRE(put(initial,other,DATA+0x2801u+alignment,4)&&put(initial,other+12u,16,4)&&
        put(initial,other+16u,16,4),"brush other descriptor");
    args[7]=0x8badf00d;
}
static void services(uint32_t base)
{
    *(volatile uint32_t *)0x1b0e4=SERVICES;
    *(volatile uint32_t *)0x1b0e0=palette_base;
    for(unsigned i=0;i<1280;++i)*(volatile uint8_t *)(SERVICES+i)=(uint8_t)(i*37u+cases);
    *(volatile uint32_t *)(SERVICES+0)=base+(format?0x1b2f4u:0x1b26eu);
    *(volatile uint32_t *)(SERVICES+8)=base+(format?0x1bd26u:0x1ba8au);
    *(volatile uint32_t *)(SERVICES+68)=base+(accelerated?0x173dau:0x17322u);
}
static void run(void)
{
    scenario=cases+skipped;begin_oracle();uint32_t value;
    if(!oracle(&value)){++skipped;return;}
    const void *native[]={(void *)kernel_palette_lookup_81b714,(void *)kernel_brush_square_81d032,
        (void *)kernel_brush_line_81d070,(void *)kernel_brush_circle_81d1fc,(void *)kernel_circle_quadrants_81d4e6};
    const void *functions[]={(void *)(uintptr_t)(entries[kind]+0x100000u),native[kind],(void *)(uintptr_t)entries[kind]};
    for(implementation=0;implementation<3;++implementation){
        reset();services(implementation?0x800000u:0x900000u);uint32_t actual[9];
        for(unsigned i=0;i<1280;++i)saved_services[i]=*(volatile uint8_t *)(SERVICES+i);
        REQUIRE(!raster_checked_call(functions[implementation],args,actual),"brush ABI");
        compare(actual,value);
        for(unsigned i=0;i<1280;++i)REQUIRE(saved_services[i]==*(volatile uint8_t *)(SERVICES+i),"brush service table mutation");
        REQUIRE(*(volatile uint32_t *)0x1b0e0==palette_base&&*(volatile uint32_t *)0x1b0e4==SERVICES,"brush globals");
    }
    for(unsigned i=0;i<8;++i)if(get32(expected,ARGUMENTS+i*4u)!=args[i])++changed_words;
    ++counts[kind];++cases;
}
void _start(void)
{
    mappings();REQUIRE(map(0x1b000,4096)&&map(SERVICES,4096),"brush service maps");
    REQUIRE(load("build/raster_brush_original.x",0x900000)&&load("build/raster_brush_candidate.x",0x800000),"brush images");
    stage=1;kind=0;
    for(alignment=0;alignment<4;++alignment)for(unsigned index=0;index<16;++index){
        pattern();for(unsigned i=0;i<8;++i)args[i]=0;
        args[0]=values[index];
        palette_base=DATA+0x1001u+alignment-args[0]*4u;run();
    }
    stage=2;kind=1;
    static const uint32_t sides[]={0,1,2,3,5,0xffffffffu,0x80000000u};
    for(format=0;format<2;++format)for(accelerated=0;accelerated<(format?2u:1u);++accelerated)
      for(alignment=0;alignment<4;++alignment)for(unsigned w=0;w<16;++w)
      for(unsigned h=0;h<16;++h)for(unsigned side=0;side<7;++side){
        args[0]=ids[(w+h+side)%16u];args[1]=w%3u?2u:0xfffffffeu;args[2]=h%3u?3u:0xffffffffu;
        args[3]=random32();args[4]=sides[side];args[5]=0x13579bdf;args[6]=0x2468ace0;
        setup(values[w],values[h],DATA+0x1001u+alignment);run();
    }
    stage=3;kind=2;
    static const uint32_t origins[][2]={{0,0},{4,3},{0xfffffffcu,0xfffffffeu},
        {0x3ffffcu,0x200001u},{0x7ffffffcu,0x80000003u},{0x80000000u,0x7fffffffu}};
    for(format=0;format<2;++format)for(accelerated=0;accelerated<(format?2u:1u);++accelerated)
      for(alignment=0;alignment<4;++alignment)for(unsigned o=0;o<6;++o)
      for(unsigned dx=0;dx<9;++dx)for(unsigned dy=0;dy<9;++dy)for(unsigned side=0;side<3;++side){
        args[0]=ids[(o+dx+dy)%16u];args[1]=origins[o][0];args[2]=origins[o][1];
        args[3]=args[1]+dx-4u;args[4]=args[2]+dy-4u;args[5]=random32();args[6]=side;
        setup(16,16,DATA+0x1001u+alignment);run();
    }
    stage=4;kind=3;
    static const uint32_t radii[]={0,1,2,3,7,16,0xffffffffu,0x80000000u};
    for(format=0;format<2;++format)for(accelerated=0;accelerated<(format?2u:1u);++accelerated)
      for(alignment=0;alignment<4;++alignment)for(unsigned dim=0;dim<16;++dim)
      for(unsigned origin=0;origin<3;++origin)for(unsigned r=0;r<8;++r){
        args[0]=ids[(dim+origin+r)%16u];args[1]=origin*5u-1u;args[2]=origin*4u-1u;
        args[3]=radii[r];args[4]=random32();args[5]=(dim+origin)%4u;args[6]=0x2468ace0;
        setup(values[dim],values[(dim+3u)%16u],DATA+0x1001u+alignment);run();
    }
    stage=5;kind=4;
    for(format=0;format<2;++format)for(alignment=0;alignment<4;++alignment)
      for(unsigned r=0;r<8;++r)for(unsigned mask=0;mask<32;++mask)for(unsigned origin=0;origin<3;++origin){
        args[0]=ids[(r+mask+origin)%16u];args[1]=origin*5u-1u;args[2]=origin*4u-1u;
        args[3]=radii[r];args[4]=mask|((mask&16u)?0xffff0000u:0u);args[5]=random32();args[6]=0x2468ace0;
        setup(16,16,DATA+0x1001u+alignment);run();
    }
    stage=6;
    for(kind=1;kind<5;++kind)for(format=0;format<2;++format)
      for(accelerated=0;accelerated<(format?2u:1u);++accelerated)
      for(alignment=0;alignment<4;++alignment)for(unsigned at=0;at<32;++at){
        for(unsigned i=0;i<7;++i)args[i]=0;
        args[3]=kind==3?1u:0u;args[4]=kind==4?15u:1u;
        args[5]=kind==1?0x13579bdfu:1u;args[6]=kind==2?1u:0x2468ace0u;
        setup(0,16,ARGUMENTS+at);run();
    }
    for(kind=1;kind<5;++kind)for(format=0;format<2;++format)
      for(alignment=0;alignment<4;++alignment)for(unsigned alias=0;alias<9;++alias){
        accelerated=0;for(unsigned i=0;i<7;++i)args[i]=0;
        uint32_t surface=DATA+0x101u+alignment;
        uint32_t dest=alias<6u?surface+alias*4u:0x500000u+(alias-6u)*4u;
        uint32_t color=alias==0?DATA+0x2801u+alignment:alias>=6?DATA+0x201u+alignment:4u;
        if(kind==1){args[3]=color;args[4]=1;}
        if(kind==2){args[3]=1;args[5]=color;args[6]=1;}
        if(kind==3){args[3]=1;args[4]=color;args[5]=1;}
        if(kind==4){args[4]=15;args[5]=color;}
        setup(3,3,dest);run();
    }
    REQUIRE(caller_stores&&metadata_stores&&changed_words&&zero_circles&&branch_counts[0]&&branch_counts[1]&&block_stores&&row_copies,"brush coverage");
    PRINT("Brush scenarios palette/square/line/circle/quadrants:");
    for(unsigned i=0;i<5;++i){PRINT(" ");number(counts[i]);}
    PRINT("; executions: ");number(cases*3u);PRINT("; excluded unsafe/large: ");number(skipped);
    PRINT("; metadata/caller stores/changed words: ");number(metadata_stores);PRINT(" ");number(caller_stores);PRINT(" ");number(changed_words);
    PRINT("; zero-radius EAX cases: ");number(zero_circles);PRINT("; blocks/row copies: ");number(block_stores);PRINT(" ");number(row_copies);
    PRINT("; brush primitives verified against 32 KiB memory/eight caller words/raw EAX/ABI oracle; real fill/pixel/copy callees, no CALL substitutions\n");
    syscall3(1,0,0,0);
}
