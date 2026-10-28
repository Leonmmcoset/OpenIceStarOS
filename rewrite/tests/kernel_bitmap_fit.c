#define BITMAP_CHAIN
#include "kernel_bitmap.c"
#undef BITMAP_CHAIN
static uint32_t fit_paths[2],fit_skip_x,fit_repeat_x,fit_skip_y,fit_repeat_y,
    fit_clamps,fit_budget_exclusions,fit_faults,fit_loops,fit_reads,fit_globals;
static uint32_t fit_captured_duplicates,fit_dither_phase;
static int field(unsigned offset,uint32_t *value)
{
    uint32_t s;if(!record(incoming(0),&s)||!valid(s+offset,4))return 0;
    *value=read_bytes(s+offset,4);return 1;
}
static int rgb_source(uint32_t file,uint32_t header,uint32_t x,uint32_t y,uint32_t *p)
{
    if(!valid(header+8u,4)||!valid(header+4u,4)||!valid(file+10u,4))return 0;
    uint64_t row=(uint32_t)(read_bytes(header+8u,4)-y-1u);
    *p=(uint32_t)((uint64_t)incoming(1)+read_bytes(file+10u,4)+
        (row*read_bytes(header+4u,4)+x)*3u);return 1;
}
static int fit_emit(uint32_t p,uint32_t x,uint32_t y,uint32_t dither_x,unsigned mode)
{
    unsigned bytes=mode?3u:4u;if(!valid(p,bytes))return 0;
    uint32_t color=mode?quantize(read_bytes(p+2u,1),read_bytes(p+1u,1),read_bytes(p,1),dither_x,y):read_bytes(p,4)%0x1000000u;
    return pixel(incoming(0),x,y,color,mode?1u:4u);
}
static int ratio(uint32_t source,uint32_t destination,uint32_t *out)
{
    if(source==destination){*out=0;return 1;}
    uint32_t gap=source>destination?source-destination:destination-source;
    int64_t numerator=signed32((uint32_t)((uint64_t)destination*100u)),denominator=signed32(gap);
    if(numerator==-2147483648LL&&denominator==-1)return 0;
    uint32_t dividend=(uint32_t)(numerator<0?-numerator:numerator),
        divisor=(uint32_t)(denominator<0?-denominator:denominator),quotient=0;
    uint64_t remainder=0;
    for(unsigned bit=32;bit;){
        --bit;remainder=remainder*2u+((dividend>>bit)&1u);
        if(remainder>=divisor){remainder-=divisor;quotient|=1u<<bit;}
    }
    *out=(numerator<0)!=(denominator<0)?0u-quotient:quotient;return 1;
}
static int threshold(uint32_t position,uint32_t count,uint32_t step)
{
    uint32_t left=(uint32_t)(((uint64_t)position+1u)*100u),right=(uint32_t)((uint64_t)count*step);
    return signed32(left)>=signed32(right);
}
static int fit_model(uint32_t *result)
{
    uint32_t file=args[1],header=file+14u,iw,ih,w,h,rx,ry,s;
    if(!valid(header+4u,4)||!valid(header+8u,4)||!field(12,&w)||!field(16,&h))return 0;
    iw=read_bytes(header+4u,4);ih=read_bytes(header+8u,4);
    if(!ratio(iw,w,&rx)||!ratio(ih,h,&ry)||!record(incoming(0),&s)||!valid(s+36u,4))return 0;
    uint32_t depth=read_bytes(s+36u,4),mode;
    if(depth!=32u&&depth!=8u){*result=s;return 1;}
    if(!valid(header+14u,2))return 0;
    if(read_bytes(header+14u,2)!=24u){*result=header;return 1;}
    mode=depth==8u;uint32_t row=0,sy=0,ycount=1,budget=0;
#define FIT_STEP() do{if(++budget>4096u){++fit_budget_exclusions;return 0;}}while(0)
#define FIT_DIM(offset,value) do{if(!field(offset,&value))return 0;}while(0)
#define FIT_HEADER(offset,value) do{if(!valid(header+offset,4))return 0;value=read_bytes(header+offset,4);}while(0)
    for(;;){
        FIT_STEP();FIT_DIM(16,h);
        if(signed32(h)<=signed32(row)){++fit_paths[mode];*result=h;return 1;}
        FIT_HEADER(8u,ih);if(sy>=ih){sy=ih-1u;++fit_clamps;}
        uint32_t column=0,sx=0,xcount=1;
        for(;;){
            FIT_STEP();FIT_DIM(12,w);if(signed32(w)<=signed32(column))break;
            FIT_HEADER(4u,iw);if(sx>=iw){sx=iw-1u;++fit_clamps;}
            uint32_t p;if(!rgb_source(file,header,sx,sy,&p)||!fit_emit(p,column,row,column,mode))return 0;
            FIT_DIM(12,w);FIT_HEADER(4u,iw);
            if(iw>w){
                while(threshold(column,xcount,rx)){
                    FIT_STEP();FIT_DIM(12,w);if(signed32(column+1u)>=signed32(w))break;
                    ++sx;++xcount;++fit_skip_x;
                }
            }else if(iw<w){
                while(threshold(column,xcount,rx)){
                    FIT_STEP();FIT_DIM(12,w);if(signed32(column+1u)>=signed32(w))break;
                    uint32_t duplicate=p;
                    if(!mode&&!rgb_source(file,header,sx,sy,&duplicate))return 0;
                    if(mode){
                        uint32_t fresh;if(!rgb_source(file,header,sx,sy,&fresh))return 0;
                        if(fresh!=p)++fit_captured_duplicates;
                        if(!valid(p,3))return 0;
                        uint32_t r=read_bytes(p+2u,1),g=read_bytes(p+1u,1),b=read_bytes(p,1);
                        if(quantize(r,g,b,column,row)!=quantize(r,g,b,column+1u,row))++fit_dither_phase;
                    }
                    if(!fit_emit(duplicate,column+1u,row,column,mode))return 0;
                    ++column;++xcount;++fit_repeat_x;
                }
            }
            ++column;++sx;
        }
        FIT_DIM(16,h);FIT_HEADER(8u,ih);
        if(ih>h){
            while(threshold(row,ycount,ry)){
                FIT_STEP();FIT_DIM(16,h);if(signed32(row+1u)>=signed32(h))break;
                ++sy;++ycount;++fit_skip_y;
            }
        }else if(ih<h&&threshold(row,ycount,ry)){
            FIT_DIM(16,h);if(signed32(row+1u)<signed32(h)){--sy;++ycount;++fit_repeat_y;}
        }
        ++row;++sy;
    }
#undef FIT_STEP
#undef FIT_DIM
#undef FIT_HEADER
}
static int fit_oracle(uint32_t *value)
{
    uint32_t *counters[]={&writes,&bitmap_stores,&metadata_stores,&caller_stores,&skipped_pixels,
        &fit_paths[0],&fit_paths[1],&fit_skip_x,&fit_repeat_x,&fit_skip_y,&fit_repeat_y,&fit_clamps,
        &fit_captured_duplicates,&fit_dither_phase};
    uint32_t saved[sizeof(counters)/sizeof(counters[0])];
    for(unsigned i=0;i<sizeof(counters)/sizeof(counters[0]);++i)saved[i]=*counters[i];
    if(fit_model(value))return 1;
    for(unsigned i=0;i<sizeof(counters)/sizeof(counters[0]);++i)*counters[i]=saved[i];
    return 0;
}
static uint32_t fit_table[320];
static void fit_setup(uint32_t width,uint32_t height,uint32_t depth,uint32_t bits,
    uint32_t image_width,uint32_t image_height,uint32_t destination)
{
    args[0]=ids[cases%16u];args[1]=0;args[2]=0;
    setup(width,height,depth,bits,image_width,image_height,destination);
    args[1]=args[3];for(unsigned i=2;i<6;++i)args[i]=random32();
}
static void fit_exercise(void)
{
    scenario=cases+skipped;begin_oracle();uint32_t value;
    if(!fit_oracle(&value)){++skipped;return;}
    static const uint32_t depths[]={0,8,15,16,24,32};
    for(implementation=0;implementation<3;++implementation){
        REQUIRE(load(implementation?"build/bitmap_fit_candidate.x":"build/bitmap_fit_original.x",0x800000),"fit images");
        *(volatile uint32_t *)0x1b0e4=0x620000;*(volatile uint32_t *)0x1b034=0x610000;
        *(volatile uint32_t *)0x610024=depths[cases%6u];*(volatile uint32_t *)0x1a868=cases/6u%2u?1u<<25:0;
        for(unsigned i=0;i<320;++i)*(volatile uint32_t *)(0x620000u+i*4u)=i*0x1234567u+cases;
        ((void (*)(void))0x8005aa)();
        REQUIRE(*(volatile uint32_t *)(0x620000u+76u*4u)==0x8204acu,"fit initialized slot");
        for(unsigned i=0;i<320;++i){
            uint32_t entry=*(volatile uint32_t *)(0x620000u+i*4u);
            if(!implementation)fit_table[i]=entry;
            else REQUIRE(entry==fit_table[i],"fit service table");
        }
        for(unsigned i=0;i<4096;++i)globals_initial[i]=*(volatile uint8_t *)(0x1b000u+i);
        reset();uint32_t actual[7];
        const void *fn=implementation==1u?(void *)kernel_bitmap_fit_8204ac:(void *)(uintptr_t)*(volatile uint32_t *)(0x620000u+76u*4u);
        REQUIRE(!raster_checked_call(fn,args,actual),"fit ABI");compare(actual,value);
        for(unsigned i=0;i<320;++i)REQUIRE(*(volatile uint32_t *)(0x620000u+i*4u)==fit_table[i],"fit table mutation");
        for(unsigned i=0;i<4096;++i)REQUIRE(*(volatile uint8_t *)(0x1b000u+i)==globals_initial[i],"fit globals");
    }
    for(unsigned i=0;i<6;++i)if(incoming(i)!=args[i])++changed_caller;
    if(incoming(1)!=args[1])++changed_bitmap;
    ++cases;
}
static void fit_guards(void)
{
    stage=9;alignment=0;
    for(unsigned probe=0;probe<8;++probe){
        unsigned mode=probe>=3u&&probe<=5u?8u:32u,trailing=probe%3u+1u;
        fit_setup(probe==6u?2u:1u,1,mode,24,probe==6u?1000u:1u,1,DATA+0x2000u);
        if(probe<6u)REQUIRE(put(initial,bitmap_base+10u,DATA+16384u-trailing-bitmap_base,4),"fit fetch guard");
        for(implementation=0;implementation<3;++implementation){
            REQUIRE(load(implementation?"build/bitmap_fit_candidate.x":"build/bitmap_fit_original.x",0x800000),"fit guard image");
            reset();int pid=syscall3(2,0,0,0);REQUIRE(pid>=0,"fit guard fork");
            if(!pid){
                if(probe==6u)syscall3(27,1,0,0);
                if(probe==7u)REQUIRE(!syscall3(91,0x1b000,4096,0),"fit global unmap");
                uint32_t actual[7];const void *fn=implementation==1u?(void *)kernel_bitmap_fit_8204ac:(void *)0x8204ac;
                raster_checked_call(fn,args,actual);syscall3(1,0,0,0);
            }
            uint32_t status=0;REQUIRE(syscall3(7,(uint32_t)pid,(uint32_t)&status,0)==pid,"fit guard wait");
            unsigned signal=status&127u;
            if(probe==5u){REQUIRE(!status,"fit 8-bit three-byte source read");++fit_reads;}
            else if(probe==6u){REQUIRE(signal==14u,"fit zero-ratio loop");++fit_loops;}
            else if(probe==7u){REQUIRE(signal==11u,"fit required global read");++fit_globals;}
            else{REQUIRE(signal==11u,"fit source fetch fault");++fit_faults;}
        }
    }
}
void _start(void)
{
    mappings();REQUIRE(map(0x1a000,8192)&&map(0x610000,4096)&&map(0x620000,4096),"fit maps");
    stage=1;
    for(unsigned mode=0;mode<2;++mode)for(unsigned w=0;w<9;++w)for(unsigned h=0;h<9;++h)
      for(unsigned iw=0;iw<9;++iw)for(unsigned ih=0;ih<9;++ih){
        alignment=(w+h+iw+ih)&3u;fit_setup(w,h,mode?8u:32u,24,iw,ih,DATA+0x2001u+alignment);fit_exercise();
    }
    stage=2;static const uint32_t depths[]={0,8,15,16,24,32},bits[]={0,1,4,8,16,24,32,0xffff};
    for(unsigned depth=0;depth<6;++depth)for(unsigned b=0;b<8;++b)
      for(unsigned w=0;w<16;++w)for(unsigned h=0;h<16;++h){
        alignment=(w+h)&3u;fit_setup(values[w],values[h],depths[depth],bits[b],3,3,DATA+0x2001u+alignment);fit_exercise();
    }
    stage=3;
    for(unsigned mode=0;mode<2;++mode)for(unsigned w=0;w<16;++w)for(unsigned h=0;h<16;++h){
        alignment=(w+h)&3u;fit_setup(3,3,mode?8u:32u,24,values[w],values[h],DATA+0x2001u+alignment);fit_exercise();
    }
    stage=4;
    for(unsigned mode=0;mode<2;++mode)for(alignment=0;alignment<4;++alignment)
      for(unsigned w=1;w<6;++w)for(unsigned h=1;h<6;++h)for(unsigned alias=0;alias<17;++alias){
        uint32_t s=DATA+0x101u+alignment,b=DATA+0x401u+alignment;
        uint32_t destination=alias<6u?ARGUMENTS+alias*4u:alias<10u?s+(alias-6u)*4u:
            alias<12u?s+36u+(alias-10u)*4u:alias<14u?0x500000u+(alias-12u)*4u:
            alias<16u?b+10u+(alias-14u)*8u:b+0x1000u;
        fit_setup(w,h,mode?8u:32u,24,3,3,destination);fit_exercise();
    }
    stage=5;alignment=0;
    fit_setup(6,1,8,24,1,1,ARGUMENTS+4u);
    /* The first store changes bitmap's low byte to 0x10. The next source
     * is captured before further stores change its remaining bytes. */
    for(unsigned i=0;i<3;++i){
        REQUIRE(put(initial,bitmap_base+0x1000u+i,0,1)&&
            put(initial,DATA+0x1410u+i,0,1),"captured 8-bit duplicate source");
    }
    fit_exercise();fit_guards();
    REQUIRE(fit_paths[0]&&fit_paths[1]&&fit_skip_x&&fit_repeat_x&&fit_skip_y&&fit_repeat_y&&fit_clamps&&changed_caller&&changed_bitmap,"fit coverage");
    PRINT("Bitmap fit scenarios/executions/excluded unsafe/budget exclusions: ");number(cases);PRINT(" ");number(cases*3u);PRINT(" ");number(skipped);PRINT(" ");number(fit_budget_exclusions);
    PRINT("; modes/skip X/repeat X/skip Y/repeat Y/clamps: ");number(fit_paths[0]);PRINT(" ");number(fit_paths[1]);PRINT(" ");number(fit_skip_x);PRINT(" ");number(fit_repeat_x);PRINT(" ");number(fit_skip_y);PRINT(" ");number(fit_repeat_y);PRINT(" ");number(fit_clamps);
    PRINT("; writes/bitmap/metadata/caller/changed caller/bitmap argument: ");number(writes);PRINT(" ");number(bitmap_stores);PRINT(" ");number(metadata_stores);PRINT(" ");number(caller_stores);PRINT(" ");number(changed_caller);PRINT(" ");number(changed_bitmap);
    PRINT("; source faults/zero-ratio loops/three-byte reads/required global faults: ");number(fit_faults);PRINT(" ");number(fit_loops);PRINT(" ");number(fit_reads);PRINT(" ");number(fit_globals);
    PRINT("; captured source duplicates/distinct dither phases: ");number(fit_captured_duplicates);PRINT(" ");number(fit_dither_phase);
    REQUIRE(fit_captured_duplicates&&fit_dither_phase,"8-bit captured source/previous-column dither coverage");
    PRINT("; bitmap fit verified with real pixel/quantizer/initializer/slot 76/320 service words/caller/EAX/ABI; no helper substitutions\n");
    syscall3(1,0,0,0);
}
