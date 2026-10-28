/* Composition oracle: child glyph arguments are snapshots; pixel output
 * can change the outer string pointer, geometry, colors and future bytes. */
#define WIDE_CHAIN
#include "kernel_text_wide.c"
#undef WIDE_CHAIN
#include "../kernel/text_run.h"
static const uint32_t run_entries[]={0x81e7b2,0x81e87c,0x81e952,0x81ea1c};
static uint32_t run_counts[4],ascii_calls,wide_calls,text_stores,ascii_font_stores,
    empty_runs,limit255,limit256,ascii_font_base,text_base,guard_faults;
static int run_store(uint32_t address,uint32_t color,uint32_t bytes)
{
    if(address-text_base<512u)++text_stores;
    if(address-ascii_font_base<2048u)++ascii_font_stores;
    return write_bytes(address,color,bytes);
}
static int child_record(uint32_t id,uint32_t *surface)
{
    uint32_t slot=(uint32_t)(0x500000ULL+(uint64_t)id*4u);
    if(!valid(slot,4))return 0;
    *surface=word(slot);return valid(*surface+12u,4);
}
static int child_ascii(uint32_t id,uint32_t x,uint32_t y,uint32_t foreground,
    uint32_t background,unsigned character)
{
    ++ascii_calls;uint32_t surface;
    if(!child_record(id,&surface))return 0;
    if(signed32(word(surface+12u))<=signed32(x))return 1;
    if(!valid(surface+16u,4))return 0;
    if(signed32(word(surface+16u))<=signed32(y)||(character&255u)>=128u)return 1;
    uint32_t font=ascii_font_base+(character&255u)*16u;
    for(unsigned row=0;row<16;++row){
        if(!valid(font+row,1))return 0;
        unsigned bits=byte_from(expected,font_expected,font+row);
        for(unsigned column=0;column<8;++column){
            if(signed32(x+column)>=signed32(word(surface+12u))||
               signed32(y+row)>=signed32(word(surface+16u))){++clipped_pixels;continue;}
            if(!valid(surface,4))return 0;
            uint32_t px=x+((kind&1u)?row:column),py=(kind&1u)?y-column:y+row;
            uint32_t bytes=kind<2?1u:4u;
            uint32_t address=(uint32_t)((uint64_t)word(surface)+((uint64_t)py*word(surface+12u)+px)*bytes);
            unsigned set=!!(bits&(0x80u>>column));
            if(!run_store(address,set?foreground:background,bytes))return 0;
            if(set)++foreground_pixels;else ++background_pixels;
        }
    }
    return 1;
}
static int child_wide(uint32_t id,uint32_t x,uint32_t y,uint32_t foreground,
    uint32_t background,uint32_t pointer)
{
    ++wide_calls;uint32_t surface;
    if(!child_record(id,&surface)||!valid(pointer,2))return 0;
    unsigned high=byte_from(expected,font_expected,pointer),low=byte_from(expected,font_expected,pointer+1u);
    if(signed32(word(surface+12u))<=signed32(x))return 1;
    if(!valid(surface+16u,4))return 0;
    if(signed32(word(surface+16u))<=signed32(y)||high<160||high==255||low<160||low==255)return 1;
    uint32_t font=tile(high,low);
    for(unsigned row=0;row<16;++row)for(unsigned column=0;column<16;++column){
        if(signed32(x+column)>=signed32(word(surface+12u))||signed32(y+row)>=signed32(word(surface+16u))){++clipped_pixels;continue;}
        if(!valid(surface,4)||!valid(font+row*2u+column/8u,1))return 0;
        unsigned bits=byte_from(expected,font_expected,font+row*2u+column/8u);
        unsigned set=!!(bits&(0x80u>>(column%8u)));
        uint32_t bytes=kind<2?1u:4u;
        uint32_t address=(uint32_t)((uint64_t)word(surface)+((uint64_t)(y+row)*word(surface+12u)+x+column)*bytes);
        if(!run_store(address,set?foreground:background,bytes))return 0;
        if(set)++foreground_pixels;else ++background_pixels;
    }
    return 1;
}
static int run_model(uint32_t *result)
{
    uint32_t index=0;
    for(;;){
        uint32_t p=incoming(5)+index;
        if(!valid(p,1))return 0;
        unsigned first=byte_from(expected,font_expected,p);
        if(!first||index>254u)break;
        uint32_t x=incoming(1)+((kind&1u)?0u:index*8u);
        uint32_t y=incoming(2)-((kind&1u)?index*8u:0u);
        if(first<160u){
            if(!child_ascii(incoming(0),x,y,incoming(3),incoming(4),first))return 0;
            ++index;
        }else{
            if(!child_wide(incoming(0),x,y,incoming(3),incoming(4),p))return 0;
            index+=2u;
        }
    }
    if(!index)++empty_runs;
    if(index==255u)++limit255;
    if(index==256u)++limit256;
    *result=index;return 1;
}
static int run_oracle(uint32_t *result)
{
    uint32_t *counters[]={&writes[0],&writes[1],&writes[2],&writes[3],&metadata_writes,&caller_writes,
        &code_writes,&font_writes,&foreground_pixels,&background_pixels,&clipped_pixels,
        &ascii_calls,&wide_calls,&text_stores,&ascii_font_stores,&empty_runs,&limit255,&limit256},saved[18];
    for(unsigned i=0;i<18;++i)saved[i]=*counters[i];
    for(unsigned i=0;i<FONT_SIZE/4u;++i)font_expected[i]=font_initial[i];
    if(run_model(result))return 1;
    for(unsigned i=0;i<18;++i)*counters[i]=saved[i];
    return 0;
}
static void run_setup(uint32_t width,uint32_t height,uint32_t destination)
{
    pattern();uint32_t surface=DATA+0x101u+alignment,other=DATA+0x201u+alignment;
    uint32_t offset=(uint32_t)(((uint64_t)args[2]*width+args[1])*(kind<2?1u:4u));
    code_pointer=text_base=DATA+0x901u+alignment;args[5]=text_base;
    ascii_font_base=DATA+0x3001u+alignment;
    REQUIRE(put(initial,0x500000u+args[0]*4u,surface,4)&&put(initial,surface,destination-offset,4)&&
        put(initial,surface+12u,width,4)&&put(initial,surface+16u,height,4),"run fixture");
    if((args[0]&0x3fffffffu)!=1u)REQUIRE(put(initial,0x500004u,other,4),"run second ID");
    REQUIRE(put(initial,other,DATA+0x1801u+alignment,4)&&put(initial,other+12u,24,4)&&put(initial,other+16u,24,4),"run second surface");
}
static void text_byte(unsigned offset,unsigned value)
{REQUIRE(put(initial,text_base+offset,value,1),"run input bytes");}
static void run_case(void)
{
    scenario=cases+skipped;begin_oracle();uint32_t value;
    if(!run_oracle(&value)){++skipped;return;}
    const void *native[]={(void *)kernel_text_run8_81e7b2,(void *)kernel_text_vertical8_81e87c,
        (void *)kernel_text_run32_81e952,(void *)kernel_text_vertical32_81ea1c};
    const void *functions[]={(void *)(uintptr_t)(run_entries[kind]+0x100000u),native[kind],(void *)(uintptr_t)run_entries[kind]};
    for(implementation=0;implementation<3;++implementation){
        reset();font_reset();*(volatile uint32_t *)0x1b020=ascii_font_base;uint32_t actual[7];
        REQUIRE(!raster_checked_call(functions[implementation],args,actual),"text run ABI");
        compare(actual,value);font_compare();
        REQUIRE(*(volatile uint32_t *)0x1b020==ascii_font_base,"run font global");
    }
    for(unsigned i=0;i<6;++i)if(incoming(i)!=args[i])++changed_words;
    ++run_counts[kind];++cases;
}
static void read_limit_guards(void)
{
    stage=9;
    for(kind=0;kind<4;++kind)for(unsigned size=255;size<=256;++size){
        alignment=0;args[0]=0;args[1]=24;args[2]=24;args[3]=1;args[4]=0;
        run_setup(24,24,DATA+0x1801u);
        args[5]=DATA+16384u-size;
        for(unsigned i=0;i<size;++i)REQUIRE(put(initial,args[5]+i,i==254u&&size==256u?161u:65u,1),"run guard bytes");
        const void *native[]={(void *)kernel_text_run8_81e7b2,(void *)kernel_text_vertical8_81e87c,
            (void *)kernel_text_run32_81e952,(void *)kernel_text_vertical32_81ea1c};
        const void *functions[]={(void *)(uintptr_t)(run_entries[kind]+0x100000u),native[kind],(void *)(uintptr_t)run_entries[kind]};
        for(implementation=0;implementation<3;++implementation){
            reset();*(volatile uint32_t *)0x1b020=ascii_font_base;
            int pid=syscall3(2,0,0,0);REQUIRE(pid>=0,"run guard fork");
            if(!pid){uint32_t actual[7];raster_checked_call(functions[implementation],args,actual);syscall3(1,0,0,0);}
            uint32_t status=0;REQUIRE(syscall3(7,(uint32_t)pid,(uint32_t)&status,0)==pid,"run guard wait");
            REQUIRE((status&127u)==11u,"mandatory byte read before index limit");++guard_faults;
        }
    }
}
#ifdef RUN_CHAIN
void run_direct_start(void)
#else
void _start(void)
#endif
{
    wide_mappings();REQUIRE(map(0x1b000,4096),"run globals");
    REQUIRE(load("build/text_run_original.x",0x900000)&&load("build/text_run_candidate.x",0x800000),"run images");
    stage=1;static const unsigned seconds[]={0,65,159,160,161,254,255};
    for(kind=0;kind<4;++kind)for(alignment=0;alignment<4;++alignment)
      for(unsigned first=0;first<256;++first)for(unsigned second=0;second<7;++second){
        args[0]=ids[(first+second)%16u];args[1]=2;args[2]=8;args[3]=random32();args[4]=random32();
        run_setup(24,24,DATA+0x1801u+alignment);text_byte(0,first);text_byte(1,seconds[second]);text_byte(2,0);run_case();
    }
    stage=2;
    for(kind=0;kind<4;++kind)for(unsigned length=0;length<=257;++length)for(unsigned mode=0;mode<3;++mode){
        alignment=length&3u;args[0]=ids[length%16u];args[1]=2;args[2]=8;args[3]=random32();args[4]=random32();
        run_setup(24,24,DATA+0x1801u+alignment);
        for(unsigned i=0;i<length;++i)text_byte(i,mode==0?65u:mode==1?161u:i%3u?161u:65u);
        text_byte(length,0);text_byte(length+1u,0);run_case();
    }
    stage=3;
    for(kind=0;kind<4;++kind)for(alignment=0;alignment<4;++alignment)
      for(unsigned w=0;w<16;++w)for(unsigned h=0;h<16;++h){
        args[0]=ids[(w+h)%16u];args[1]=w&1u?0xffffffffu:0;args[2]=h&1u?0xfffffffeu:8;
        args[3]=random32();args[4]=random32();run_setup(values[w],values[h],DATA+0x1801u+alignment);
        text_byte(0,65);text_byte(1,161);text_byte(2,161);text_byte(3,159);text_byte(4,0);run_case();
    }
    stage=4;
    for(kind=0;kind<4;++kind)for(alignment=0;alignment<4;++alignment)for(unsigned alias=0;alias<11;++alias){
        uint32_t surface=DATA+0x101u+alignment;
        uint32_t dest=alias<6u?surface+alias*4u:alias<9u?0x500000u+(alias-6u)*4u:
            alias==9u?DATA+0x901u+alignment:DATA+0x3001u+alignment+65u*16u;
        uint32_t color=alias==0?DATA+0x1801u+alignment:alias>=6&&alias<9?DATA+0x201u+alignment:4u;
        args[0]=0;args[1]=0;args[2]=8;args[3]=color;args[4]=color;
        run_setup(24,24,dest);text_byte(0,65);text_byte(1,161);text_byte(2,161);text_byte(3,0);run_case();
    }
    for(kind=0;kind<4;++kind)for(alignment=0;alignment<4;++alignment)for(unsigned at=0;at<21;++at){
        args[0]=0;args[1]=0;args[2]=0;args[3]=1;args[4]=0;run_setup(1,1,ARGUMENTS+at);
        text_byte(0,65);text_byte(1,161);text_byte(2,161);text_byte(3,0);run_case();
    }
    REQUIRE(ascii_calls&&wide_calls&&text_stores&&ascii_font_stores&&caller_writes&&metadata_writes&&
        changed_words&&empty_runs&&limit255&&limit256,"run coverage");
    read_limit_guards();
    PRINT("Text-run scenarios horizontal8/vertical8/horizontal32/vertical32:");for(unsigned i=0;i<4;++i){PRINT(" ");number(run_counts[i]);}
    PRINT("; executions: ");number(cases*3u);PRINT("; excluded unsafe: ");number(skipped);
    PRINT("; ASCII/wide calls: ");number(ascii_calls);PRINT(" ");number(wide_calls);
    PRINT("; text/ASCII-font/metadata/caller stores/changed words: ");number(text_stores);PRINT(" ");number(ascii_font_stores);PRINT(" ");number(metadata_writes);PRINT(" ");number(caller_writes);PRINT(" ");number(changed_words);
    PRINT("; empty/255/256 counts/guard faults: ");number(empty_runs);PRINT(" ");number(limit255);PRINT(" ");number(limit256);PRINT(" ");number(guard_faults);
    PRINT("; text runs verified against 32 KiB memory/284 KiB font/six caller words/raw EAX/ABI and mandatory-read fault oracles; real ASCII/wide C or original callees; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
