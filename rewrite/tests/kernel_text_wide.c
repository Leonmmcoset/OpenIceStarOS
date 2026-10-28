/* Independent 16-bit row-mask / widened-address oracle. Font memory is
 * separately mapped and compared in full, including the accepted 0xa0
 * characters that address memory before the nominal font-table base. */
#include "../kernel/text_wide.h"
#include "raster_fixture.h"
#define FONT_START 0x1f000u
#define FONT_SIZE 0x47000u
static uint32_t font_initial[FONT_SIZE/4u],font_expected[FONT_SIZE/4u];
static uint32_t kind,alignment,glyph_counts[4],writes[4],metadata_writes,
    caller_writes,code_writes,font_writes,changed_words,live_font_reads,
    foreground_pixels,background_pixels,transparent_pixels,clipped_pixels,
    accepted,rejected,negative_tiles,code_pointer;
static const uint32_t entries[]={0x81e106,0x81e2cc,0x81e45a,0x81e624};
static int valid(uint32_t a,uint32_t bytes)
{
    for(unsigned i=0;i<bytes;++i)
        if(a+i-FONT_START>=FONT_SIZE&&index_of(a+i)==UINT32_MAX)return 0;
    return 1;
}
static uint8_t byte_from(const uint8_t *memory,const uint32_t *font,uint32_t a)
{return a-FONT_START<FONT_SIZE?((const uint8_t *)font)[a-FONT_START]:memory[index_of(a)];}
static uint32_t word(uint32_t a)
{
    uint32_t value=0;
    for(unsigned i=0;i<4;++i)value|=(uint32_t)byte_from(expected,font_expected,a+i)<<(8u*i);
    return value;
}
static int write_bytes(uint32_t a,uint32_t color,uint32_t bytes)
{
    if(!valid(a,bytes))return 0;
    for(unsigned i=0;i<bytes;++i){
        if(a+i-FONT_START<FONT_SIZE)((uint8_t *)font_expected)[a+i-FONT_START]=(uint8_t)(color>>(8u*i));
        else expected[index_of(a+i)]=(uint8_t)(color>>(8u*i));
    }
    ++writes[kind];
    if(a-ARGUMENTS<24u)++caller_writes;
    if(a-DATA<0x1000u||a-0x500000u<4096u)++metadata_writes;
    if(a-code_pointer<2u)++code_writes;
    if(a-FONT_START<FONT_SIZE)++font_writes;
    return 1;
}
static uint32_t incoming(unsigned i){return get32(expected,ARGUMENTS+i*4u);}
static uint32_t tile(unsigned high,unsigned low)
{
    int64_t cell=((int64_t)high-161)*94+(int64_t)low-161;
    return (uint32_t)(0x20200LL+cell*32);
}
static int wide_model(uint32_t *result)
{
    uint32_t slot=(uint32_t)(0x500000ULL+(uint64_t)args[0]*4u);
    if(!valid(slot,4))return 0;
    uint32_t surface=word(slot),pointer=args[kind&1u?4u:5u];
    if(!valid(pointer,2))return 0;
    unsigned high=byte_from(expected,font_expected,pointer),low=byte_from(expected,font_expected,pointer+1u);
    if(!valid(surface+12u,4))return 0;
    if(signed32(word(surface+12u))<=signed32(args[1]))goto reject;
    if(!valid(surface+16u,4))return 0;
    if(signed32(word(surface+16u))<=signed32(args[2]))goto reject;
    if(high<160||high==255||low<160||low==255)goto reject;
    uint32_t font=tile(high,low);
    if(font<0x20200u)++negative_tiles;
    for(unsigned row=0;row<16;++row)for(unsigned column=0;column<16;++column){
        uint32_t xx=incoming(1)+column,yy=incoming(2)+row;
        if(signed32(xx)>=signed32(word(surface+12u))||signed32(yy)>=signed32(word(surface+16u))){++clipped_pixels;continue;}
        uint32_t font_address=font+row*2u+column/8u;
        if(!valid(font_address,1))return 0;
        unsigned bits=byte_from(expected,font_expected,font_address);
        if(bits!=byte_from(initial,font_initial,font_address))++live_font_reads;
        /* Interpret the two font bytes as a high-bit-first 16-bit row. */
        unsigned row_mask=column<8?bits<<8:bits;
        int set=!!(row_mask&(0x8000u>>column));
        if(!set&&(kind&1u)){++transparent_pixels;continue;}
        if(!valid(surface,4))return 0;
        uint32_t color=incoming(set?3u:4u),bytes=kind<2?1u:4u;
        uint32_t out=(uint32_t)((uint64_t)word(surface)+
            ((uint64_t)(incoming(2)+row)*word(surface+12u)+incoming(1)+column)*bytes);
        if(!write_bytes(out,color,bytes))return 0;
        if(set)++foreground_pixels;else ++background_pixels;
    }
    ++accepted;*result=1;return 1;
reject:
    ++rejected;*result=0;return 1;
}
static int wide_oracle(uint32_t *result)
{
    uint32_t *counters[]={&writes[0],&writes[1],&writes[2],&writes[3],&metadata_writes,&caller_writes,
        &code_writes,&font_writes,&live_font_reads,&foreground_pixels,&background_pixels,
        &transparent_pixels,&clipped_pixels,&accepted,&rejected,&negative_tiles},saved[16];
    for(unsigned i=0;i<16;++i)saved[i]=*counters[i];
    for(unsigned i=0;i<FONT_SIZE/4u;++i)font_expected[i]=font_initial[i];
    if(wide_model(result))return 1;
    for(unsigned i=0;i<16;++i)*counters[i]=saved[i];
    return 0;
}
static void setup(uint32_t width,uint32_t height,uint32_t destination,unsigned high,unsigned low)
{
    pattern();uint32_t surface=DATA+0x101u+alignment;
    uint32_t offset=(uint32_t)(((uint64_t)args[2]*width+args[1])*(kind<2?1u:4u));
    code_pointer=DATA+0x901u+alignment;
    args[kind&1u?4u:5u]=code_pointer;
    if(kind&1u)args[5]=0x8badf00du;
    REQUIRE(put(initial,code_pointer,high,1)&&put(initial,code_pointer+1u,low,1)&&
        put(initial,0x500000u+args[0]*4u,surface,4)&&put(initial,surface,destination-offset,4)&&
        put(initial,surface+12u,width,4)&&put(initial,surface+16u,height,4),"wide fixture");
}
static void font_reset(void)
{for(unsigned i=0;i<FONT_SIZE/4u;++i)*(volatile uint32_t *)(FONT_START+i*4u)=font_initial[i];}
static void font_compare(void)
{for(unsigned i=0;i<FONT_SIZE/4u;++i)REQUIRE(*(volatile uint32_t *)(FONT_START+i*4u)==font_expected[i],"whole font memory oracle");}
static void wide_mappings(void)
{
    mappings();REQUIRE(map(FONT_START,FONT_SIZE),"wide font map");
    for(unsigned i=0;i<FONT_SIZE/4u;++i)font_initial[i]=(i*0x1749927u)^0x89ab1357u^(i<<21);
}
static void run(void)
{
    scenario=cases+skipped;begin_oracle();uint32_t value;
    if(!wide_oracle(&value)){++skipped;return;}
    const void *native[]={(void *)kernel_text_wide8_81e106,(void *)kernel_text_wide8_mask_81e2cc,
        (void *)kernel_text_wide32_81e45a,(void *)kernel_text_wide32_mask_81e624};
    const void *functions[]={(void *)(uintptr_t)(entries[kind]+0x100000u),native[kind],(void *)(uintptr_t)entries[kind]};
    for(implementation=0;implementation<3;++implementation){
        reset();font_reset();uint32_t actual[RASTER_ARGUMENT_WORDS+1];
        REQUIRE(!raster_checked_call(functions[implementation],args,actual),"wide ABI");
        compare(actual,value);font_compare();
    }
    for(unsigned i=0;i<6;++i)if(incoming(i)!=args[i])++changed_words;
    ++glyph_counts[kind];++cases;
}
#ifdef WIDE_CHAIN
void wide_direct_start(void)
#else
void _start(void)
#endif
{
    wide_mappings();REQUIRE(load("build/text_wide_original.x",0x900000)&&load("build/text_wide_candidate.x",0x800000),"wide images");
    stage=1;
    for(kind=0;kind<4;++kind)for(unsigned high=160;high<255;++high)for(unsigned low=160;low<255;++low){
        alignment=(high+low)&3u;args[0]=ids[(high+low)%16u];args[1]=2;args[2]=3;
        args[3]=random32();args[4]=random32();setup(24,24,DATA+0x1801u+alignment,high,low);run();
    }
    stage=2;
    static const uint32_t origins[][2]={{0,0},{0xffffffffu,0xfffffffeu},{0x7fffffffu,0x80000000u}};
    static const unsigned codes[][2]={{160,160},{161,161},{255,161}};
    for(kind=0;kind<4;++kind)for(alignment=0;alignment<4;++alignment)
      for(unsigned w=0;w<16;++w)for(unsigned h=0;h<16;++h)
      for(unsigned origin=0;origin<3;++origin)for(unsigned code=0;code<3;++code){
        args[0]=ids[(w+h+origin+code)%16u];args[1]=origins[origin][0];args[2]=origins[origin][1];
        args[3]=random32();args[4]=random32();
        setup(values[w],values[h],DATA+0x1801u+alignment,codes[code][0],codes[code][1]);run();
    }
    stage=3;
    for(kind=0;kind<4;++kind)for(unsigned byte=0;byte<256;++byte)for(unsigned swap=0;swap<2;++swap){
        alignment=byte&3u;args[0]=0;args[1]=0;args[2]=0;args[3]=random32();args[4]=random32();
        setup(24,24,DATA+0x1801u+alignment,swap?161u:byte,swap?byte:161u);run();
    }
    stage=4;
    for(kind=0;kind<4;++kind)for(alignment=0;alignment<4;++alignment)
      for(unsigned alias=0;alias<9;++alias){
        uint32_t surface=DATA+0x101u+alignment;
        uint32_t dest=alias<6?surface+alias*4u:0x500000u+(alias-6u)*4u;
        uint32_t color=alias==0?DATA+0x1801u+alignment:alias>=6?DATA+0x201u+alignment:4u;
        args[0]=0;args[1]=0;args[2]=0;args[3]=color;args[4]=color;
        setup(3,3,dest,161,161);run();
    }
    for(kind=0;kind<4;++kind)for(alignment=0;alignment<4;++alignment)for(unsigned at=0;at<21;++at){
        args[0]=0;args[1]=0;args[2]=0;args[3]=1;args[4]=0;
        setup(1,1,ARGUMENTS+at,161,161);run();
    }
    stage=5;
    for(kind=0;kind<4;++kind)for(alignment=0;alignment<4;++alignment)for(unsigned code=0;code<8;++code){
        unsigned high=code?160u+code:254u,low=code?254u-code:160u;
        args[0]=0;args[1]=2;args[2]=3;args[3]=0;args[4]=0xfefefefeu;
        uint32_t font=tile(high,low);
        for(unsigned i=0;i<8;++i)font_initial[(font-FONT_START)/4u+i]=0xffffffffu;
        setup(24,24,font,high,low);run();
        setup(24,24,DATA+0x901u+alignment,high,low);run();
    }
    stage=6;
    for(kind=0;kind<4;++kind){
        alignment=0;args[0]=0;args[1]=0;args[2]=0;args[3]=1;args[4]=0;
        setup(1,1,DATA+0x1801u,161,161);uint32_t surface=DATA+16384u-16u;
        REQUIRE(put(initial,0x500000u,surface,4)&&put(initial,surface+12u,0,4),"wide guard");run();
    }
    REQUIRE(metadata_writes&&caller_writes&&code_writes&&font_writes&&changed_words&&live_font_reads&&
        foreground_pixels&&background_pixels&&transparent_pixels&&clipped_pixels&&accepted&&rejected&&negative_tiles,"wide coverage");
    PRINT("Wide scenarios opaque8/masked8/opaque32/masked32:");
    for(unsigned i=0;i<4;++i){PRINT(" ");number(glyph_counts[i]);}
    PRINT("; executions: ");number(cases*3u);PRINT("; excluded unsafe: ");number(skipped);
    PRINT("; foreground/background/transparent/clipped: ");number(foreground_pixels);PRINT(" ");number(background_pixels);PRINT(" ");number(transparent_pixels);PRINT(" ");number(clipped_pixels);
    PRINT("; metadata/caller/code/font stores/changed words/live font reads: ");number(metadata_writes);PRINT(" ");number(caller_writes);PRINT(" ");number(code_writes);PRINT(" ");number(font_writes);PRINT(" ");number(changed_words);PRINT(" ");number(live_font_reads);
    PRINT("; accepted/rejected/below-base tiles: ");number(accepted);PRINT(" ");number(rejected);PRINT(" ");number(negative_tiles);
    PRINT("; wide glyphs verified against 32 KiB memory/284 KiB font/six caller words/raw EAX/ABI oracle; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
