/* Radix digits are predicted using increasing powers, then composed with
 * the independent real-glyph memory oracle. The renderer receives a snapshot
 * of outer arguments; stores into the number caller cannot change that copy. */
#define RASTER_ARGUMENT_WORDS 8
#define RUN_CHAIN
#include "kernel_text_run.c"
#undef RUN_CHAIN
#include "../kernel/text_number.h"
static const uint32_t number_entries[]={0x81eaf2,0x81ec98,0x81ee3e};
static uint32_t number_counts[3],faults,loops,wrapped_digits,embedded_zero,
    wide_numbers,number_caller_changes;
static int number_wide(uint32_t id,uint32_t x,uint32_t y,uint32_t foreground,
    uint32_t background,unsigned high,unsigned low)
{
    ++wide_numbers;uint32_t surface;
    if(!child_record(id,&surface))return 0;
    if(signed32(word(surface+12u))<=signed32(x))return 1;
    if(!valid(surface+16u,4))return 0;
    if(signed32(word(surface+16u))<=signed32(y)||high<160u||high==255u||low<160u||low==255u)return 1;
    uint32_t font=tile(high,low);
    for(unsigned row=0;row<16;++row)for(unsigned column=0;column<16;++column){
        if(signed32(x+column)>=signed32(word(surface+12u))||signed32(y+row)>=signed32(word(surface+16u)))continue;
        if(!valid(surface,4)||!valid(font+row*2u+column/8u,1))return 0;
        unsigned bits=byte_from(expected,font_expected,font+row*2u+column/8u);
        uint32_t bytes=kind<2?1u:4u;
        uint32_t address=(uint32_t)((uint64_t)word(surface)+((uint64_t)(y+row)*word(surface+12u)+x+column)*bytes);
        if(!write_bytes(address,bits&(0x80u>>(column%8u))?foreground:background,bytes))return 0;
    }
    return 1;
}
static unsigned formatted(uint8_t text[34],uint32_t radix,uint32_t value)
{
    if(!value){text[0]='0';text[1]=0;return 1;}
    uint32_t power=1;unsigned length=1;
    while((uint64_t)power*radix<=value){power*=radix;++length;}
    for(unsigned i=0;i<length;++i){
        unsigned d=value/power;value-=d*power;
        unsigned c=(d+48u)&255u;
        if(c>=58u&&c<128u)c=(c+7u)&255u;
        if(c>=128u)++wrapped_digits;
        text[i]=(uint8_t)c;power/=radix;
    }
    text[length]=0;return length;
}
static int number_model(uint32_t *result)
{
    if(kind==1u){
        uint64_t power=1;uint32_t n=1;
        while(power*args[0]<=args[1]&&args[1]){power*=args[0];++n;}
        *result=n;return 1;
    }
    uint8_t text[34];unsigned length=formatted(text,args[5],args[6]);
    if(text[length-1u]>=160u)return 0; /* Would read uninitialized trailing stack bytes. */
    uint32_t index=0;
    while(text[index]){
        uint32_t x=args[1]+index*8u;
        if(text[index]<160u){
            if(!child_ascii(args[0],x,args[2],args[3],args[4],text[index]))return 0;
            ++index;
        }else{
            if(!number_wide(args[0],x,args[2],args[3],args[4],text[index],text[index+1u]))return 0;
            index+=2u;
        }
    }
    if(index<length)++embedded_zero;
    *result=index;return 1;
}
static void number_setup(uint32_t width,uint32_t height,uint32_t destination)
{
    uint32_t radix=args[5];
    run_setup(width,height,destination);args[5]=radix;
}
static int number_oracle(uint32_t *result)
{
    uint32_t *counters[]={&writes[0],&writes[1],&writes[2],&writes[3],&metadata_writes,&caller_writes,
        &code_writes,&font_writes,&foreground_pixels,&background_pixels,&clipped_pixels,
        &ascii_calls,&text_stores,&ascii_font_stores,&wrapped_digits,&embedded_zero,&wide_numbers};
    uint32_t saved[sizeof(counters)/sizeof(counters[0])];
    for(unsigned i=0;i<sizeof(counters)/sizeof(counters[0]);++i)saved[i]=*counters[i];
    for(unsigned i=0;i<FONT_SIZE/4u;++i)font_expected[i]=font_initial[i];
    if(number_model(result))return 1;
    for(unsigned i=0;i<sizeof(counters)/sizeof(counters[0]);++i)*counters[i]=saved[i];
    return 0;
}
static void number_case(void)
{
    scenario=cases+skipped;begin_oracle();
    uint32_t value;
    if(!number_oracle(&value)){++skipped;return;}
    const void *native[]={(void *)kernel_text_number8_81eaf2,
        (void *)kernel_text_number32_81ec98,(void *)kernel_number_digits_81ee3e};
    unsigned entry=kind==1u?2u:kind/2u;
    const void *functions[]={(void *)(uintptr_t)(number_entries[entry]+0x100000u),
        native[entry],(void *)(uintptr_t)number_entries[entry]};
    for(implementation=0;implementation<3;++implementation){
        reset();font_reset();*(volatile uint32_t *)0x1b020=ascii_font_base;
        uint32_t actual[9];
        REQUIRE(!raster_checked_call(functions[implementation],args,actual),"number ABI");
        compare(actual,value);font_compare();
        REQUIRE(*(volatile uint32_t *)0x1b020==ascii_font_base,"number font global");
    }
    for(unsigned i=0;i<8;++i)if(incoming(i)!=args[i])++number_caller_changes;
    ++number_counts[entry];++cases;
}
static void exceptional(void)
{
    stage=9;
    for(unsigned entry=0;entry<3;++entry)for(unsigned mode=0;mode<2;++mode){
        kind=entry==2u?1u:entry*2u;alignment=0;
        args[0]=0;args[1]=0;args[2]=0;args[3]=1;args[4]=0;args[5]=mode;args[6]=17;args[7]=0;
        number_setup(1,1,DATA+0x1801u);
        if(entry==2u){args[0]=mode;args[1]=17;}
        const void *native[]={(void *)kernel_text_number8_81eaf2,
            (void *)kernel_text_number32_81ec98,(void *)kernel_number_digits_81ee3e};
        const void *functions[]={(void *)(uintptr_t)(number_entries[entry]+0x100000u),native[entry],
            (void *)(uintptr_t)number_entries[entry]};
        for(implementation=0;implementation<3;++implementation){
            reset();int pid=syscall3(2,0,0,0);REQUIRE(pid>=0,"number fork");
            if(!pid){
                if(mode)syscall3(27,1,0,0);
                uint32_t actual[9];raster_checked_call(functions[implementation],args,actual);
                syscall3(1,0,0,0);
            }
            uint32_t status=0;REQUIRE(syscall3(7,(uint32_t)pid,(uint32_t)&status,0)==pid,"number wait");
            REQUIRE((status&127u)==(mode?14u:8u),"original radix-zero fault/radix-one loop");
            if(mode)++loops;else ++faults;
        }
    }
}
#ifdef NUMBER_CHAIN
void number_direct_start(void)
#else
void _start(void)
#endif
{
    wide_mappings();REQUIRE(map(0x1b000,4096),"number globals");
    REQUIRE(load("build/text_number_original.x",0x900000)&&load("build/text_number_candidate.x",0x800000),"number images");
    stage=1;
    for(kind=0;kind<=2;kind+=2)for(unsigned radix=2;radix<=40;++radix)
      for(unsigned value=0;value<256;++value){
        alignment=(radix+value)&3u;args[0]=ids[value%16u];args[1]=1;args[2]=2;
        args[3]=random32();args[4]=random32();args[5]=radix;args[6]=value;args[7]=random32();
        number_setup(24,24,DATA+0x1801u+alignment);number_case();
    }
    stage=2;static const uint32_t bases[]={0,1,2,3,8,10,16,36,37,79,80,81,112,128,159,160,207,208,209,255,256,257,65535,0x80000000u,0xffffffffu};
    for(kind=0;kind<=2;kind+=2)for(unsigned b=0;b<25;++b)for(unsigned n=0;n<16;++n){
        if(b<2u&&values[n])continue;
        alignment=n&3u;args[0]=0;args[1]=0;args[2]=0;args[3]=random32();args[4]=random32();
        args[5]=bases[b];args[6]=values[n];args[7]=random32();number_setup(24,24,DATA+0x1801u+alignment);number_case();
    }
    stage=3;
    for(kind=0;kind<=2;kind+=2)for(unsigned byte=0;byte<256;++byte)for(unsigned mode=0;mode<3;++mode){
        alignment=byte&3u;args[0]=0;args[1]=0;args[2]=0;args[3]=random32();args[4]=random32();
        args[5]=256;args[6]=mode==0u?byte:mode==1u?byte*256u+10u:byte*65536u+113u*256u+10u;args[7]=0;
        number_setup(24,24,DATA+0x1801u+alignment);number_case();
    }
    for(kind=0;kind<=2;kind+=2)for(alignment=0;alignment<4;++alignment)
      for(unsigned at=0;at<29;++at){
        args[0]=0;args[1]=0;args[2]=0;args[3]=1;args[4]=0;args[5]=16;args[6]=0x12345678u;args[7]=random32();
        number_setup(1,1,ARGUMENTS+at);number_case();
    }
    stage=4;
    for(kind=0;kind<=2;kind+=2)for(alignment=0;alignment<4;++alignment)
      for(unsigned alias=0;alias<10;++alias){
        uint32_t surface=DATA+0x101u+alignment;
        uint32_t dest=alias<6u?surface+alias*4u:alias<9u?0x500000u+(alias-6u)*4u:DATA+0x3001u+alignment+'1'*16u;
        uint32_t color=alias==0?DATA+0x1801u+alignment:alias>=6u&&alias<9u?DATA+0x201u+alignment:4u;
        args[0]=0;args[1]=0;args[2]=0;args[3]=color;args[4]=color;args[5]=10;args[6]=111;args[7]=0;
        number_setup(24,24,dest);number_case();
    }
    stage=5;
    for(kind=0;kind<=2;kind+=2)for(unsigned w=0;w<16;++w)for(unsigned h=0;h<16;++h){
        alignment=(w+h)&3u;args[0]=ids[(w+h)%16u];args[1]=w&1u?0xffffffffu:0;
        args[2]=h&1u?0xfffffffeu:0;args[3]=1;args[4]=0;args[5]=16;args[6]=0xffffffffu;args[7]=0;
        number_setup(values[w],values[h],DATA+0x1801u+alignment);number_case();
    }
    stage=6;kind=1;
    for(unsigned b=0;b<25;++b)for(unsigned n=0;n<512;++n){
        if(b<2u&&n)continue;
        args[0]=bases[b];args[1]=n<256u?n:random32();
        args[2]=random32();args[3]=random32();args[4]=random32();args[5]=random32();args[6]=random32();args[7]=random32();
        number_case();
    }
    exceptional();
    REQUIRE(number_caller_changes&&wrapped_digits&&embedded_zero&&faults==9&&loops==9,"number coverage");
    PRINT("Number scenarios 8-bit/32-bit/digit-count:");
    for(unsigned i=0;i<3;++i){PRINT(" ");number(number_counts[i]);}
    PRINT("; executions: ");number(cases*3u);PRINT("; excluded unsafe: ");number(skipped);
    PRINT("; wrapped digits/embedded zero/wide pairs/caller changes: ");number(wrapped_digits);PRINT(" ");number(embedded_zero);PRINT(" ");number(wide_numbers);PRINT(" ");number(number_caller_changes);
    PRINT("; fault/loop subprocess probes: ");number(faults);PRINT(" ");number(loops);
    PRINT("; numbers verified with real glyph chains/32 KiB state/284 KiB font/eight caller words/EAX/ABI; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
