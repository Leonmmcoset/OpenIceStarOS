#define BITMAP_CHAIN
#include "kernel_bitmap.c"
#undef BITMAP_CHAIN
#define SERVICES 0x620000u
static uint32_t reference_table[320];
void _start(void)
{
    mappings();REQUIRE(map(0x1a000,8192)&&map(0x610000,4096)&&map(SERVICES,4096),"bitmap chain maps");
    uint32_t total=0,excluded=0;stage=10;
    static const uint32_t depths[]={0,8,15,16,24,32};
    for(unsigned depth=0;depth<6;++depth)for(unsigned accelerated=0;accelerated<2;++accelerated)
      for(unsigned mode=0;mode<4;++mode)for(alignment=0;alignment<4;++alignment)
      for(unsigned alias=0;alias<8;++alias){
        args[0]=ids[(mode*4u+alias)%16u];args[1]=0;args[2]=0;
        uint32_t destination=alias<4u?DATA+0x2001u+alignment:
            alias<6u?ARGUMENTS+(alias-4u)*4u:
            DATA+0x401u+alignment+(alias==6u?0x1000u:54u);
        setup(3,3,mode==3u?8u:32u,mode==0u||mode==3u?24u:mode==1u?8u:4u,3,3,destination);
        scenario=total+excluded;begin_oracle();uint32_t value;
        if(!oracle(&value)){++excluded;continue;}
        for(implementation=0;implementation<2;++implementation){
            REQUIRE(load(implementation?"build/bitmap_candidate.x":"build/bitmap_original.x",0x800000),"bitmap chain image");
            *(volatile uint32_t *)0x1b0e4=SERVICES;*(volatile uint32_t *)0x1b034=0x610000;
            *(volatile uint32_t *)0x610024=depths[depth];*(volatile uint32_t *)0x1a868=accelerated?1u<<25:0;
            for(unsigned i=0;i<320;++i)*(volatile uint32_t *)(SERVICES+i*4u)=i*0x1234567u+cases;
            ((void (*)(void))0x8005aa)();
            REQUIRE(*(volatile uint32_t *)(SERVICES+75u*4u)==0x81ffc4u,"bitmap initialized slot");
            for(unsigned i=0;i<320;++i){
                uint32_t entry=*(volatile uint32_t *)(SERVICES+i*4u);
                if(!implementation)reference_table[i]=entry;
                else REQUIRE(entry==reference_table[i],"bitmap entire initialized table");
            }
            for(unsigned i=0;i<4096;++i)globals_initial[i]=*(volatile uint8_t *)(0x1b000u+i);
            reset();uint32_t actual[7];
            REQUIRE(!raster_checked_call((void *)(uintptr_t)*(volatile uint32_t *)(SERVICES+75u*4u),args,actual),"bitmap chain ABI");
            compare(actual,value);
            for(unsigned i=0;i<4096;++i)REQUIRE(*(volatile uint8_t *)(0x1b000u+i)==globals_initial[i],"bitmap chain globals");
            for(unsigned i=0;i<320;++i)REQUIRE(*(volatile uint32_t *)(SERVICES+i*4u)==reference_table[i],"bitmap chain table mutation");
        }
        ++total;++cases;
    }
    PRINT("Bitmap initialized-service scenarios/executions/excluded unsafe: ");number(total);PRINT(" ");number(total*2u);PRINT(" ");number(excluded);
    PRINT("; bitmap renderer verified through real initializer/six display depths/two CPU choices/slot 75/independent surface depth; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
