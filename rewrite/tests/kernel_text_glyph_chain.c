/* Select each orientation through the real mode-dependent table slots. */
#define GLYPH_CHAIN
#include "kernel_text_glyph.c"
#undef GLYPH_CHAIN
#define SERVICES 0x620000u
static uint32_t reference_table[320];
void _start(void)
{
    mappings();REQUIRE(map(0x1a000,8192)&&map(0x610000,4096)&&map(SERVICES,4096),"glyph chain maps");
    uint32_t total=0;stage=10;
    static const uint32_t depths[]={0,8,15,16,24,32};
    for(unsigned depth=0;depth<6;++depth)for(unsigned accelerated=0;accelerated<2;++accelerated)
      for(unsigned rotated=0;rotated<2;++rotated)for(alignment=0;alignment<4;++alignment)
      for(unsigned glyph=0;glyph<256;++glyph){
        kind=(depths[depth]==32?2u:0u)+rotated;
        args[0]=ids[glyph%16u];args[1]=2;args[2]=8;args[3]=random32();args[4]=random32();args[5]=glyph|0x9bad0000u;
        setup(16,24,DATA+0x1801u+alignment);scenario=total;begin_oracle();uint32_t value;
        REQUIRE(glyph_oracle(&value),"glyph chain oracle");
        for(implementation=0;implementation<2;++implementation){
            REQUIRE(load(implementation?"build/text_glyph_candidate.x":"build/text_glyph_original.x",0x800000),"glyph chain kernel");
            *(volatile uint32_t *)0x1b0e4=SERVICES;*(volatile uint32_t *)0x1b034=0x610000;
            *(volatile uint32_t *)0x610024=depths[depth];*(volatile uint32_t *)0x1b020=font_base;
            *(volatile uint32_t *)0x1a868=accelerated?1u<<25:0;
            for(unsigned i=0;i<320;++i)*(volatile uint32_t *)(SERVICES+i*4u)=i*0x1234567u+cases;
            *(volatile uint32_t *)(SERVICES+6u*4u)=entries[0];
            *(volatile uint32_t *)(SERVICES+12u*4u)=entries[1];
            ((void (*)(void))0x8005aa)();
            unsigned slot=rotated?12u:6u;
            REQUIRE(*(volatile uint32_t *)(SERVICES+slot*4u)==entries[kind],"glyph initialized slot");
            for(unsigned i=0;i<320;++i){
                uint32_t word=*(volatile uint32_t *)(SERVICES+i*4u);
                if(!implementation)reference_table[i]=word;
                else REQUIRE(word==reference_table[i],"glyph entire initialized table");
            }
            reset();uint32_t actual[7];
            REQUIRE(!raster_checked_call((void *)(uintptr_t)*(volatile uint32_t *)(SERVICES+slot*4u),args,actual),"glyph chain ABI");
            compare(actual,value);
            for(unsigned i=0;i<320;++i)REQUIRE(*(volatile uint32_t *)(SERVICES+i*4u)==reference_table[i],"glyph chain table mutation");
        }
        ++total;++cases;
    }
    PRINT("Glyph initialized-service scenarios: ");number(total);PRINT("; executions: ");number(total*2u);
    PRINT("; text glyphs verified through real initializer/six depths/two CPU selections/slots 6 and 12; unsupported depths retain seeded real 8-bit callees; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
