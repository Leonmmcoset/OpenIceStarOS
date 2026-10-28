/* Actual service initializer -> common slots 237..241 -> unmodified chain. */
#define _start raster_brush_unit_start
#include "kernel_raster_brush.c"
#undef _start
static uint32_t reference_table[320];
void _start(void)
{
    mappings();REQUIRE(map(0x1a000,8192)&&map(SERVICES,4096)&&map(0x610000,4096),"brush chain maps");
    uint32_t total=0,excluded=0;stage=10;
    for(kind=0;kind<5;++kind)for(format=0;format<2;++format)
      for(accelerated=0;accelerated<2;++accelerated)for(alignment=0;alignment<4;++alignment)
      for(unsigned r=0;r<8;++r)for(unsigned mode=0;mode<4;++mode){
        args[0]=ids[r%16u];args[1]=mode;args[2]=mode;
        args[3]=kind==2?mode+r:kind==1?random32():r;
        args[4]=kind==1?r:kind==2?mode+3u:kind==3?random32():((1u<<mode)|0xff00u);
        args[5]=kind==2||kind==4?random32():2u;args[6]=kind==2?mode:0x2468ace0;
        setup(16,16,DATA+0x1001u+alignment);
        palette_base=DATA+0x1001u+alignment-args[0]*4u;
        scenario=total+excluded;begin_oracle();uint32_t value;
        if(!oracle(&value)){++excluded;continue;}
        for(implementation=0;implementation<2;++implementation){
            REQUIRE(load(implementation?"build/raster_brush_candidate.x":"build/raster_brush_original.x",0x800000),"brush chain image");
            *(volatile uint32_t *)0x1b0e4=SERVICES;
            *(volatile uint32_t *)0x1b034=0x610000;
            *(volatile uint32_t *)0x1b0e0=palette_base;
            *(volatile uint32_t *)0x610024=format?32u:8u;
            *(volatile uint32_t *)0x1a868=accelerated?1u<<25:0;
            for(unsigned i=0;i<320;++i)*(volatile uint32_t *)(SERVICES+i*4u)=i*0x1234567u+cases;
            ((void (*)(void))0x8005aa)();
            REQUIRE(*(volatile uint32_t *)(SERVICES+(237u+kind)*4u)==entries[kind],"brush initialized slot");
            for(unsigned i=0;i<320;++i){
                uint32_t word=*(volatile uint32_t *)(SERVICES+i*4u);
                if(!implementation)reference_table[i]=word;
                else REQUIRE(word==reference_table[i],"brush entire initialized table");
            }
            reset();uint32_t actual[9];
            REQUIRE(!raster_checked_call((void *)(uintptr_t)*(volatile uint32_t *)(SERVICES+(237u+kind)*4u),args,actual),"brush chain ABI");
            compare(actual,value);
            for(unsigned i=0;i<320;++i)REQUIRE(*(volatile uint32_t *)(SERVICES+i*4u)==reference_table[i],"brush chain table mutation");
        }
        ++total;++cases;
    }
    PRINT("Brush initialized-service scenarios: ");number(total);PRINT("; executions: ");number(total*2u);
    PRINT("; excluded large/unmapped: ");number(excluded);
    PRINT("; brush primitives verified through real initializer/eight-and-32-bit mode/CPU selection/slots 237..241/fill/pixel/copy chain; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
