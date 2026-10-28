/* Byte-address oracle for the original unusual clipping/rotation rules.
 * Font bits are decoded using masks, and addresses use widened products.
 * Stores can change font bytes, captured descriptors, ID slots and caller
 * words. No drawing helpers or substituted CALL targets are involved. */
#include "../kernel/text_glyph.h"
#include "raster_fixture.h"
static uint32_t kind,alignment,font_base,glyph_counts[4],writes[4],
    metadata_writes,caller_writes,font_writes,changed_words,changed_font_rows,
    foreground_pixels,background_pixels,clipped_pixels,accepted,rejected,global_writes,changed_font_pointers;
static uint8_t global_initial[4096],global_expected[4096];
static const uint32_t entries[]={0x81db5a,0x81dcc0,0x81de2a,0x81df96};
static int valid(uint32_t a,uint32_t bytes)
{
    for(uint32_t i=0;i<bytes;++i)
        if(a+i-0x1b000u>=4096u&&index_of(a+i)==UINT32_MAX)return 0;
    return 1;
}
static uint8_t glyph_byte(const uint8_t *memory,const uint8_t *globals,uint32_t a)
{return a-0x1b000u<4096u?globals[a-0x1b000u]:memory[index_of(a)];}
static uint32_t glyph_word(uint32_t a)
{
    uint32_t value=0;
    for(unsigned i=0;i<4;++i)value|=(uint32_t)glyph_byte(expected,global_expected,a+i)<<(8u*i);
    return value;
}
static int glyph_put(uint32_t a,uint32_t color,uint32_t bytes)
{
    if(!valid(a,bytes))return 0;
    for(unsigned i=0;i<bytes;++i){
        uint32_t address=a+i;
        if(address-0x1b000u<4096u)global_expected[address-0x1b000u]=(uint8_t)(color>>(8u*i));
        else expected[index_of(address)]=(uint8_t)(color>>(8u*i));
    }
    if(a-0x1b000u<4096u)++global_writes;
    return 1;
}
static void glyph_global_pattern(void)
{
    for(unsigned i=0;i<4096;++i)global_initial[i]=(uint8_t)(i*53u+cases*71u);
    for(unsigned i=0;i<4;++i)global_initial[32u+i]=(uint8_t)(font_base>>(8u*i));
    for(unsigned i=0;i<4096;++i)global_expected[i]=global_initial[i];
}
static uint32_t incoming(unsigned i){return get32(expected,ARGUMENTS+i*4u);}
static int glyph_model(uint32_t *result)
{
    uint32_t slot=(uint32_t)(0x500000ULL+(uint64_t)args[0]*4u);
    if(!valid(slot,4))return 0;
    uint32_t surface=glyph_word(slot);
    if(!valid(surface+12u,4))return 0;
    if(signed32(glyph_word(surface+12u))<=signed32(args[1]))goto reject;
    if(!valid(surface+16u,4))return 0;
    if(signed32(glyph_word(surface+16u))<=signed32(args[2]))goto reject;
    uint32_t glyph=args[5]&255u;
    if(glyph>=128u)goto reject;
    for(unsigned row=0;row<16;++row){
        uint32_t address=font_base+glyph*16u+row;
        if(!valid(address,1))return 0;
        uint8_t bits=glyph_byte(expected,global_expected,address);
        if(bits!=glyph_byte(initial,global_initial,address))++changed_font_rows;
        for(unsigned column=0;column<8;++column){
            uint32_t xx=incoming(1)+column,yy=incoming(2)+row;
            if(signed32(xx)>=signed32(glyph_word(surface+12u))||
               signed32(yy)>=signed32(glyph_word(surface+16u))){++clipped_pixels;continue;}
            if(!valid(surface,4))return 0;
            uint32_t width=glyph_word(surface+12u);
            uint32_t actual_x=incoming(1)+((kind&1u)?row:column);
            uint32_t actual_y=(kind&1u)?incoming(2)-column:incoming(2)+row;
            uint32_t bytes=kind<2?1u:4u;
            uint32_t out=(uint32_t)((uint64_t)glyph_word(surface)+
                ((uint64_t)actual_y*width+actual_x)*bytes);
            int is_foreground=!!(bits&(0x80u>>column));
            uint32_t color=incoming(is_foreground?3u:4u);
            if(!glyph_put(out,color,bytes))return 0;
            ++writes[kind];
            if(is_foreground)++foreground_pixels;else ++background_pixels;
            if(out-ARGUMENTS<24u)++caller_writes;
            if(out-DATA<0x1000u||out-0x500000u<4096u)++metadata_writes;
            if(out-font_base<2048u)++font_writes;
        }
    }
    ++accepted;*result=1;return 1;
reject:
    ++rejected;*result=0;return 1;
}
static int glyph_oracle(uint32_t *result)
{
    uint32_t *counters[]={&writes[0],&writes[1],&writes[2],&writes[3],&metadata_writes,
        &caller_writes,&font_writes,&changed_font_rows,&foreground_pixels,&background_pixels,
        &clipped_pixels,&accepted,&rejected,&global_writes},saved[14];
    for(unsigned i=0;i<14;++i)saved[i]=*counters[i];
    glyph_global_pattern();
    if(glyph_model(result))return 1;
    for(unsigned i=0;i<14;++i)*counters[i]=saved[i];
    return 0;
}
static void setup(uint32_t width,uint32_t height,uint32_t destination)
{
    pattern();uint32_t surface=DATA+0x101u+alignment;
    uint32_t offset=(uint32_t)(((uint64_t)args[2]*width+args[1])*(kind<2?1u:4u));
    REQUIRE(put(initial,0x500000u+args[0]*4u,surface,4)&&
        put(initial,surface,destination-offset,4)&&put(initial,surface+12u,width,4)&&
        put(initial,surface+16u,height,4),"glyph fixture");
    font_base=DATA+0x3001u+alignment;
}
static void run(void)
{
    scenario=cases+skipped;begin_oracle();uint32_t value;
    if(!glyph_oracle(&value)){++skipped;return;}
    const void *native[]={(void *)kernel_text_glyph8_81db5a,(void *)kernel_text_rotated8_81dcc0,
        (void *)kernel_text_glyph32_81de2a,(void *)kernel_text_rotated32_81df96};
    const void *functions[]={(void *)(uintptr_t)(entries[kind]+0x100000u),
        native[kind],(void *)(uintptr_t)entries[kind]};
    for(implementation=0;implementation<3;++implementation){
        reset();for(unsigned i=0;i<4096;++i)*(volatile uint8_t *)(0x1b000u+i)=global_initial[i];
        uint32_t actual[7];
        REQUIRE(!raster_checked_call(functions[implementation],args,actual),"glyph ABI");
        compare(actual,value);
        for(unsigned i=0;i<4096;++i)REQUIRE(*(volatile uint8_t *)(0x1b000u+i)==global_expected[i],"glyph 4 KiB globals oracle");
    }
    for(unsigned i=0;i<6;++i)if(incoming(i)!=args[i])++changed_words;
    if(glyph_word(0x1b020u)!=font_base)++changed_font_pointers;
    ++glyph_counts[kind];++cases;
}
#ifdef GLYPH_CHAIN
void glyph_direct_start(void)
#else
void _start(void)
#endif
{
    mappings();REQUIRE(map(0x1b000,4096),"glyph globals");
    REQUIRE(load("build/text_glyph_original.x",0x900000)&&load("build/text_glyph_candidate.x",0x800000),"glyph images");
    stage=1;
    /* Every low byte, with unrelated high bits. Both orientation checks run
     * against the same origin even though rotated stores go above it. */
    for(kind=0;kind<4;++kind)for(alignment=0;alignment<4;++alignment)
      for(unsigned glyph=0;glyph<256;++glyph)for(unsigned origin=0;origin<2;++origin){
        args[0]=ids[(glyph+origin)%16u];args[1]=origin?30u:2u;args[2]=origin?23u:8u;
        args[3]=random32();args[4]=random32();args[5]=glyph|(random32()&0xffffff00u);
        setup(31,24,DATA+0x1801u+alignment);run();
    }
    stage=2;
    static const uint32_t origins[][2]={{0,0},{0xffffffffu,0xfffffffeu},
        {3,4},{0x7fffffffu,0x80000000u}};
    static const uint32_t glyphs[]={0,65,127,128};
    for(kind=0;kind<4;++kind)for(alignment=0;alignment<4;++alignment)
      for(unsigned w=0;w<16;++w)for(unsigned h=0;h<16;++h)
      for(unsigned origin=0;origin<4;++origin)for(unsigned glyph=0;glyph<4;++glyph){
        args[0]=ids[(w+h+origin+glyph)%16u];args[1]=origins[origin][0];args[2]=origins[origin][1];
        args[3]=random32();args[4]=random32();args[5]=glyphs[glyph];
        setup(values[w],values[h],DATA+0x1801u+alignment);run();
    }
    stage=3;
    for(kind=0;kind<4;++kind)for(alignment=0;alignment<4;++alignment)
      for(unsigned alias=0;alias<9;++alias){
        uint32_t surface=DATA+0x101u+alignment;
        uint32_t dest=alias<6?surface+alias*4u:0x500000u+(alias-6u)*4u;
        uint32_t color=alias==0?DATA+0x1801u+alignment:alias>=6?DATA+0x201u+alignment:4u;
        args[0]=0;args[1]=0;args[2]=0;args[3]=color;args[4]=color;args[5]=65;
        setup(3,3,dest);run();
    }
    for(kind=0;kind<4;++kind)for(alignment=0;alignment<4;++alignment)
      for(unsigned at=0;at<21;++at){
        args[0]=0;args[1]=0;args[2]=0;args[3]=1;args[4]=0;args[5]=65;
        setup(1,1,ARGUMENTS+at);run();
    }
    stage=4;
    for(kind=0;kind<4;++kind)for(alignment=0;alignment<4;++alignment)
      for(unsigned glyph=0;glyph<128;++glyph){
        args[0]=0;args[1]=2;args[2]=8;args[3]=random32();args[4]=random32();args[5]=glyph;
        setup(16,24,DATA+0x3001u+alignment+glyph*16u);run();
    }
    stage=5;
    for(kind=0;kind<4;++kind)for(unsigned reject=0;reject<3;++reject){
        alignment=0;args[0]=0;args[1]=reject==0?16u:0;args[2]=reject==1?16u:0;
        args[3]=random32();args[4]=random32();args[5]=reject==2?0x80000080u:65u;
        setup(16,16,DATA+0x1801u);font_base=0x17283940;run();
    }
    /* A failed X guard must not touch the unmapped height word. */
    for(kind=0;kind<4;++kind){
        alignment=0;args[0]=0;args[1]=0;args[2]=0;args[3]=1;args[4]=0;args[5]=65;
        setup(1,1,DATA+0x1801u);uint32_t surface=DATA+16384u-16u;
        REQUIRE(put(initial,0x500000u,surface,4)&&put(initial,surface+12u,0,4),"glyph short-circuit guard");
        font_base=0x17283940;run();
    }
    stage=6;
    /* Pixel output rewrites the global font pointer during row zero. The
     * original keeps the earlier pointer for all subsequent font rows. */
    for(kind=0;kind<4;++kind)for(alignment=0;alignment<4;++alignment){
        args[0]=0;args[1]=2;args[2]=8;args[3]=0x17283940u;args[4]=0x17283940u;args[5]=65;
        uint32_t bytes=kind<2?1u:4u;
        setup(16,24,0x1b020u+((kind&1u)?7u*16u*bytes:0u));run();
    }
    stage=7;
    /* Change the caller's character byte to a negative signed character.
     * Even with all later pixels clipped, every later row still fetches
     * from the captured original glyph. Re-reading the changed byte would
     * address an unmapped page below the font buffer in this fixture. */
    for(kind=0;kind<4;++kind)for(alignment=0;alignment<4;++alignment){
        args[0]=0;args[1]=0;args[2]=0;args[3]=0x80808080u;args[4]=0x80808080u;args[5]=65;
        setup(1,1,ARGUMENTS+20u);font_base=DATA;run();
    }
    REQUIRE(metadata_writes&&caller_writes&&changed_words&&font_writes&&changed_font_rows&&
        foreground_pixels&&background_pixels&&clipped_pixels&&accepted&&rejected&&global_writes&&changed_font_pointers,"glyph coverage");
    PRINT("Glyph scenarios normal8/rotated8/normal32/rotated32:");
    for(unsigned i=0;i<4;++i){PRINT(" ");number(glyph_counts[i]);}
    PRINT("; executions: ");number(cases*3u);PRINT("; excluded unsafe: ");number(skipped);
    PRINT("; foreground/background/clipped: ");number(foreground_pixels);PRINT(" ");number(background_pixels);PRINT(" ");number(clipped_pixels);
    PRINT("; metadata/caller/font stores/changed words/font rows: ");number(metadata_writes);PRINT(" ");number(caller_writes);PRINT(" ");number(font_writes);PRINT(" ");number(changed_words);PRINT(" ");number(changed_font_rows);
    PRINT("; global stores/changed font pointers: ");number(global_writes);PRINT(" ");number(changed_font_pointers);
    PRINT("; accepted/rejected: ");number(accepted);PRINT(" ");number(rejected);
    PRINT("; text glyphs verified against 32 KiB memory/4 KiB globals/six caller words/raw EAX/ABI oracle; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
