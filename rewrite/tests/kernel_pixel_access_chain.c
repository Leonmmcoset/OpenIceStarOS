/* Run the actual service initializer and forwarding code through table slots
 * zero and nineteen to the real pixel services. Reuse the unit fixture's
 * guarded mappings, bytewise helpers and ABI wrapper; no calls are redirected.
 */
#define _start pixel_access_unit_start
#include "kernel_pixel_access.c"
#undef _start
#define SERVICES 0x620000u
#define DISPLAY 0x610000u
static uint8_t saved_services[4096];
static const uint32_t sizes[]={0,1,7,0x7fffffff,0x80000000,0xffffffff};
static const uint32_t points[]={0,1,6,7,0x7fffffff,0x80000000,0xfffffffe,0xffffffff};

void _start(void)
{
    REQUIRE(map(TABLE,TABLE_BYTES)&&map(DATA,DATA_BYTES)&&map(0x800000,0x40000)&&
            map(0x1a000,0x2000)&&map(DISPLAY,4096)&&map(SERVICES,4096),"chain mmap");
    uint32_t total=0,written=0,negative=0,sentinels=0;
    for(implementation=0;implementation<2;++implementation){
        REQUIRE(load(implementation?"build/pixel_access_chain_candidate.x":"build/pixel_access_original.x",0x800000),"chain image load");
        random_state=0x19283746u;cases[0]=cases[1]=0;
        for(uint32_t depth=0;depth<2;++depth)for(uint32_t w=0;w<6;++w)for(uint32_t h=0;h<6;++h)
          for(uint32_t x=0;x<8;++x)for(uint32_t y=0;y<8;++y){
            stage=10+depth;scenario=total;
            uint32_t width=sizes[w],height=sizes[h],px=points[x],py=points[y],bytes=depth?4u:1u;
            uint32_t address=DATA+2049u+(w+h+x+y)%4u,color=random32();
            prepare(depth,ids[(w+h+x+y)%16u],DATA+101u+(x+y)%4u,
                    width,height,px,py,address,color);
            for(uint32_t i=0;i<REGION;++i)expected[i]=initial[i];
            int accepted=signed32(width)>signed32(px)&&signed32(height)>signed32(py);
            if(accepted&&signed32(px)>=0&&signed32(py)>=0){
                put(expected,address,color,bytes);++written;
            }
            uint32_t result=UINT32_MAX;
            if(accepted){
                result=0;
                for(uint32_t i=0;i<bytes;++i)result|=(uint32_t)expected[index_of(address+i)]<<(8u*i);
                if(signed32(px)<0||signed32(py)<0)++negative;
            }else ++sentinels;
            reset();
            *(volatile uint32_t *)0x1b0e4=SERVICES;
            *(volatile uint32_t *)0x1b034=DISPLAY;
            *(volatile uint32_t *)0x1a868=0;
            *(volatile uint32_t *)(DISPLAY+36u)=depth?32u:8u;
            for(uint32_t i=0;i<4096;++i)*(volatile uint8_t *)(SERVICES+i)=(uint8_t)(i*37u+total);
            uint32_t actual;
            REQUIRE(!icon_pixels_checked_call((void *)0x8005aa,args,&actual),"service initializer ABI");
            REQUIRE(*(volatile uint32_t *)SERVICES==entries[depth],"mode-selected setter target");
            REQUIRE(*(volatile uint32_t *)(SERVICES+76u)==entries[depth+2u],"mode-selected getter target");
            for(uint32_t i=0;i<4096;++i)saved_services[i]=*(volatile uint8_t *)(SERVICES+i);
            REQUIRE(!icon_pixels_checked_call((void *)0x8374cf,args,&actual),"forwarded setter ABI");
            check_memory();
            REQUIRE(!icon_pixels_checked_call((void *)0x837939,args,&actual),"forwarded getter ABI");
            REQUIRE(actual==result,"forwarded getter result");
            check_memory();
            for(uint32_t i=0;i<4096;++i)
                REQUIRE(saved_services[i]==*(volatile uint8_t *)(SERVICES+i),"pixel service mutated dispatch table");
            ++cases[depth];++total;
          }
    }
    PRINT("Service chain original/candidate executions: ");number(total);
    PRINT("; setter writes: ");number(written);PRINT("; negative-coordinate reads: ");number(negative);
    PRINT("; rejected reads: ");number(sentinels);
    PRINT("; real initializer/mode selection/forwarders/pixel services verified; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
