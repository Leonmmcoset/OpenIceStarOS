/* Composed memory oracle for formatter state, live incoming words and copied
 * child frames. Original and candidate kernels are loaded at their real base
 * before each execution, including the real initialized service forwarders. */
#define NUMBER_CHAIN
#include "kernel_text_number.c"
#undef NUMBER_CHAIN
#include "../kernel/text_format.h"
#define SERVICES 0x620000u
static const uint32_t format_entries[]={0x81ee7e,0x81f384,0x81f88a};
static uint32_t formatter,depth_choice,format_counts[3],format_index,
    format_returns,format_args_changed,format_x_changed,format_y_changed,
    format_pointer_changed,format_zero_padding,format_signed,format_strings,format_unknown;
static uint32_t chosen_depth=UINT32_MAX,chosen_cpu,format_reference_table[320],format_table_cases;
static int format_byte(uint32_t offset,unsigned *out)
{
    uint32_t address=incoming(5)+format_index+offset;
    if(!valid(address,1))return 0;
    *out=byte_from(expected,font_expected,address);return 1;
}
static void format_advance(unsigned word_index,uint32_t delta)
{REQUIRE(put(expected,ARGUMENTS+word_index*4u,incoming(word_index)+delta,4),"format incoming update");}
static int format_ascii(unsigned character,uint32_t *result)
{
    uint32_t id=incoming(0),x=incoming(1),y=incoming(2),fg=incoming(3),bg=incoming(4),surface;
    if(!child_record(id,&surface))return 0;
    *result=0;
    if(signed32(word(surface+12u))>signed32(x)){
        if(!valid(surface+16u,4))return 0;
        *result=signed32(word(surface+16u))>signed32(y)&&character<128u;
    }
    return child_ascii(id,x,y,fg,bg,character);
}
static int format_number(uint32_t radix,uint32_t value,uint32_t *length)
{
    uint8_t digits[34];*length=formatted(digits,radix,value);
    uint32_t id=incoming(0),x=incoming(1),y=incoming(2),fg=incoming(3),bg=incoming(4);
    for(unsigned i=0;i<*length;++i)
        if(!child_ascii(id,x+i*8u,y,fg,bg,digits[i]))return 0;
    return 1;
}
static int format_run(uint32_t pointer,uint32_t *length)
{
    uint32_t id=incoming(0),x=incoming(1),y=incoming(2),fg=incoming(3),bg=incoming(4),i=0;
    for(;;){
        if(!valid(pointer+i,1))return 0;
        unsigned first=byte_from(expected,font_expected,pointer+i);
        if(!first||i>254u)break;
        if(first<160u){if(!child_ascii(id,x+i*8u,y,fg,bg,first))return 0;++i;}
        else{if(!child_wide(id,x+i*8u,y,fg,bg,pointer+i))return 0;i+=2u;}
    }
    *length=i;return 1;
}
static int format_model(uint32_t *result)
{
    uint32_t cursor=ARGUMENTS+24u;format_index=0;
    for(unsigned guard=0;guard<512;++guard){
        unsigned current;if(!format_byte(0,&current))return 0;
        if(!current){*result=incoming(5)+format_index;++format_returns;return 1;}
        if(current=='%'){
            unsigned next,after;uint32_t width=0,length;
            if(!format_byte(1,&next))return 0;
            if(next>49u&&next<=57u){
                if(!format_byte(2,&after))return 0;
                if(after=='d'||after=='D'||after=='x'||after=='X'){
                    width=next-48u;++format_index;next=after;
                }
            }
            unsigned spec=next|32u;
            if(spec=='c'){
                if(!valid(cursor,4)||!format_ascii(word(cursor)&255u,&length))return 0;
                ++format_index;cursor+=4u;
            }else if(spec=='d'||spec=='u'||spec=='x'){
                if(!valid(cursor,4))return 0;
                uint32_t value=word(cursor),radix=spec=='x'?16u:10u;
                if(spec=='d'&&signed32(value)<0){
                    value=0u-value;uint32_t ignored;
                    if(!format_ascii('-',&ignored))return 0;
                    format_advance(1,8u);++format_signed;
                }
                uint8_t digits[34];length=formatted(digits,radix,value);
                while(width>length){
                    uint32_t ignored;if(!format_ascii('0',&ignored))return 0;
                    format_advance(1,8u);--width;++format_zero_padding;
                }
                if(spec!='d'){if(!valid(cursor,4))return 0;value=word(cursor);}
                if(!format_number(radix,value,&length))return 0;
                ++format_index;cursor+=4u;
            }else if(spec=='s'){
                if(!valid(cursor,4)||!format_run(word(cursor),&length))return 0;
                ++format_index;cursor+=4u;++format_strings;
            }else{
                if(!format_byte(0,&current)||!format_ascii(current,&length))return 0;
                ++format_unknown;
            }
            format_advance(1,length*8u);
        }else if(current==13u){REQUIRE(put(expected,ARGUMENTS+4u,0,4),"format CR");}
        else if(current==10u){format_advance(2,16u);}
        else if(current<160u){uint32_t ignored;if(!format_ascii(current,&ignored))return 0;format_advance(1,8u);}
        else{
            if(!child_wide(incoming(0),incoming(1),incoming(2),incoming(3),incoming(4),incoming(5)+format_index))return 0;
            format_advance(1,16u);++format_index;
        }
        ++format_index;
    }
    return 0;
}
static int format_oracle(uint32_t *result)
{
    uint32_t *counters[]={&writes[0],&writes[1],&writes[2],&writes[3],&metadata_writes,&caller_writes,
        &code_writes,&font_writes,&foreground_pixels,&background_pixels,&clipped_pixels,
        &ascii_calls,&wide_calls,&text_stores,&ascii_font_stores,&format_returns,
        &format_zero_padding,&format_signed,&format_strings,&format_unknown};
    uint32_t saved[sizeof(counters)/sizeof(counters[0])];
    for(unsigned i=0;i<sizeof(counters)/sizeof(counters[0]);++i)saved[i]=*counters[i];
    for(unsigned i=0;i<FONT_SIZE/4u;++i)font_expected[i]=font_initial[i];
    if(format_model(result))return 1;
    for(unsigned i=0;i<sizeof(counters)/sizeof(counters[0]);++i)*counters[i]=saved[i];
    return 0;
}
static void format_setup(uint32_t width,uint32_t height,uint32_t destination,const char *text)
{
    kind=formatter==0u?0u:formatter==1u?2u:depth_choice*2u;
    run_setup(width,height,destination);
    for(unsigned i=0;i<512;++i)REQUIRE(put(initial,text_base+i,0,1),"format clear text");
    for(unsigned i=0;text[i];++i)text_byte(i,(unsigned char)text[i]);
    uint32_t string=DATA+0xc01u+alignment;
    REQUIRE(put(initial,string,0x41a1a142u,4)&&put(initial,string+4u,0,4),"format string");
}
static void format_case(void)
{
    scenario=cases+skipped;begin_oracle();uint32_t value;
    if(!format_oracle(&value)){++skipped;return;}
    const void *native[]={(void *)kernel_text_format8_81ee7e,
        (void *)kernel_text_format32_81f384,(void *)kernel_text_format_81f88a};
    for(implementation=0;implementation<3;++implementation){
        REQUIRE(load(implementation?"build/text_format_candidate.x":"build/text_format_original.x",0x800000),"format images");
        *(volatile uint32_t *)0x1b0e4=SERVICES;*(volatile uint32_t *)0x1b034=0x610000;
        *(volatile uint32_t *)0x610024=chosen_depth==UINT32_MAX?(kind?32u:8u):chosen_depth;
        *(volatile uint32_t *)0x1a868=chosen_cpu?1u<<25:0;
        for(unsigned i=0;i<320;++i)*(volatile uint32_t *)(SERVICES+i*4u)=i*0x1234567u+cases;
        static const uint32_t seeded[]={0x81db5a,0x81e106,0x81e7b2,0x81eaf2};
        for(unsigned i=0;i<4;++i)*(volatile uint32_t *)(SERVICES+(6u+i)*4u)=seeded[i];
        ((void (*)(void))0x8005aa)();
        for(unsigned i=0;i<320;++i){
            uint32_t entry=*(volatile uint32_t *)(SERVICES+i*4u);
            if(!implementation)format_reference_table[i]=entry;
            else REQUIRE(entry==format_reference_table[i],"format initialized service table");
        }
        reset();font_reset();*(volatile uint32_t *)0x1b020=ascii_font_base;
        uint32_t actual[9];const void *fn=implementation==1u?native[formatter]:(void *)(uintptr_t)format_entries[formatter];
        REQUIRE(!raster_checked_call(fn,args,actual),"format ABI");
        compare(actual,value);font_compare();
        for(unsigned i=0;i<320;++i)REQUIRE(*(volatile uint32_t *)(SERVICES+i*4u)==format_reference_table[i],"format table mutation");
    }
    for(unsigned i=0;i<8;++i)if(incoming(i)!=args[i])++format_args_changed;
    if(incoming(1)!=args[1])++format_x_changed;
    if(incoming(2)!=args[2])++format_y_changed;
    if(incoming(5)!=args[5])++format_pointer_changed;
    ++format_counts[formatter];++cases;
    if(chosen_depth!=UINT32_MAX)++format_table_cases;
}
void _start(void)
{
    wide_mappings();REQUIRE(map(0x1a000,8192)&&map(0x610000,4096)&&map(SERVICES,4096),"format globals");
    stage=1;
    static const char *texts[]={"","abc","%","%%","%c","%C","%d","%D","%u","%U","%x","%X","%s","%S",
        "%2d","%9D","%2x","%9X","%1d","%0x","%2u","%9s","%zd","%d:%x","%c\r\n%c","A\rB\nC","%s!","%9d/%u"};
    for(formatter=0;formatter<3;++formatter)for(depth_choice=0;depth_choice<(formatter==2u?2u:1u);++depth_choice)
      for(unsigned t=0;t<sizeof(texts)/sizeof(texts[0]);++t)for(unsigned n=0;n<16;++n){
        alignment=(t+n)&3u;args[0]=ids[n];args[1]=1;args[2]=2;args[3]=random32();args[4]=random32();
        args[6]=(t==12u||t==13u||t==26u)?DATA+0xc01u+alignment:values[n];args[7]=values[15u-n];
        format_setup(32,24,DATA+0x1801u+alignment,texts[t]);format_case();
    }
    stage=2;
    for(formatter=0;formatter<3;++formatter)for(depth_choice=0;depth_choice<(formatter==2u?2u:1u);++depth_choice)
      for(unsigned byte=0;byte<256;++byte)for(unsigned mode=0;mode<4;++mode){
        alignment=byte&3u;args[0]=0;args[1]=0;args[2]=0;args[3]=random32();args[4]=random32();args[6]=DATA+0xc01u+alignment;args[7]=1;
        format_setup(24,24,DATA+0x1801u+alignment,"");
        if(mode){text_byte(0,'%');text_byte(1,byte);}else{text_byte(0,byte);text_byte(1,161);}
        if(mode>=2u)text_byte(2,mode==2u?'d':'x');
        format_case();
    }
    stage=3;
    for(formatter=0;formatter<3;++formatter)for(depth_choice=0;depth_choice<(formatter==2u?2u:1u);++depth_choice)
      for(alignment=0;alignment<4;++alignment)for(unsigned at=0;at<29;++at){
        args[0]=0;args[1]=0;args[2]=0;args[3]=1;args[4]=0;args[6]=17;args[7]=65;
        format_setup(1,1,ARGUMENTS+at,"%9d%c");format_case();
    }
    stage=4;
    for(formatter=0;formatter<3;++formatter)for(depth_choice=0;depth_choice<(formatter==2u?2u:1u);++depth_choice)
      for(alignment=0;alignment<4;++alignment)for(unsigned alias=0;alias<12;++alias){
        uint32_t surface=DATA+0x101u+alignment;
        uint32_t dest=alias<6u?surface+alias*4u:alias<9u?0x500000u+(alias-6u)*4u:
            alias==9u?DATA+0x901u+alignment+1u:alias==10u?DATA+0x3001u+alignment+'A'*16u:DATA+0xc01u+alignment;
        uint32_t color=alias==0?DATA+0x1801u+alignment:alias>=6u&&alias<9u?DATA+0x201u+alignment:4u;
        args[0]=0;args[1]=0;args[2]=0;args[3]=color;args[4]=color;args[6]=DATA+0xc01u+alignment;args[7]=65;
        format_setup(24,24,dest,"A%s%c");format_case();
    }
    stage=5;
    for(formatter=0;formatter<3;++formatter)for(depth_choice=0;depth_choice<(formatter==2u?2u:1u);++depth_choice)
      for(alignment=0;alignment<4;++alignment)for(unsigned target=0;target<3;++target){
        uint32_t color=target?18u:DATA+0x901u+alignment+16u;
        args[0]=0;args[1]=0;args[2]=0;args[3]=color;args[4]=color;args[6]=1;args[7]=65;
        format_setup(1,1,ARGUMENTS+(target?24u:20u),target==0u?"A":target==1u?"%9x":"%9d");format_case();
    }
    stage=6;formatter=2;
    static const uint32_t depths[]={0,8,15,16,24,32};
    for(unsigned depth=0;depth<6;++depth)for(chosen_cpu=0;chosen_cpu<2;++chosen_cpu)
      for(unsigned mode=0;mode<16;++mode){
        chosen_depth=depths[depth];depth_choice=chosen_depth==32u;alignment=mode&3u;
        args[0]=ids[mode];args[1]=1;args[2]=2;args[3]=random32();args[4]=random32();
        args[6]=values[mode];args[7]=values[15u-mode];
        format_setup(32,24,DATA+0x1801u+alignment,"%2d:%9x");format_case();
    }
    chosen_depth=UINT32_MAX;chosen_cpu=0;stage=7;
    for(formatter=0;formatter<3;++formatter)for(depth_choice=0;depth_choice<(formatter==2u?2u:1u);++depth_choice)
      for(unsigned w=0;w<16;++w)for(unsigned h=0;h<16;++h){
        alignment=(w+h)&3u;args[0]=ids[(w+h)%16u];args[1]=values[w];args[2]=values[h];
        args[3]=1;args[4]=0;args[6]=0x80000000u;args[7]=65;
        format_setup(values[15u-w],values[15u-h],DATA+0x1801u+alignment,"%d\r\n%c");format_case();
    }
    stage=8;static const unsigned lengths[]={0,1,254,255,256,257,300};
    for(formatter=0;formatter<3;++formatter)for(depth_choice=0;depth_choice<(formatter==2u?2u:1u);++depth_choice)
      for(unsigned length=0;length<7;++length){
        alignment=length&3u;args[0]=0;args[1]=0;args[2]=0;args[3]=1;args[4]=0;args[6]=0;args[7]=0;
        format_setup(1,1,DATA+0x1801u+alignment,"");
        for(unsigned i=0;i<lengths[length];++i)text_byte(i,'A');
        format_case();
    }
    REQUIRE(format_zero_padding&&format_signed&&format_strings&&format_unknown&&format_x_changed&&format_y_changed&&format_pointer_changed&&format_args_changed,"format coverage");
    PRINT("Format scenarios 8-bit/32-bit/service:");for(unsigned i=0;i<3;++i){PRINT(" ");number(format_counts[i]);}
    PRINT("; executions: ");number(cases*3u);PRINT("; excluded unsafe: ");number(skipped);
    PRINT("; padding/signed/string/unknown dispatches: ");number(format_zero_padding);PRINT(" ");number(format_signed);PRINT(" ");number(format_strings);PRINT(" ");number(format_unknown);
    PRINT("; changed incoming words/X/Y/format pointer: ");number(format_args_changed);PRINT(" ");number(format_x_changed);PRINT(" ");number(format_y_changed);PRINT(" ");number(format_pointer_changed);
    PRINT("; six-depth/two-CPU table scenarios: ");number(format_table_cases);
    PRINT("; formats verified against live caller/text/font/metadata/EAX/ABI oracle with real initialized services; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
