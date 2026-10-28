/* Composed color-key image/text oracle. All pixel, service, formatter and
 * glyph bodies execute from the real original/candidate kernel image. */
#define BITMAP_CHAIN
#include "kernel_bitmap.c"
#undef BITMAP_CHAIN
static uint32_t key_paths[2],key_matches,key_pixels,key_text_calls,key_text_stores,
    key_font_stores,key_font_reloads,key_color_changes,key_changed_words,
    key_changed_keys,key_unsafe_budget,key_guard_faults,key_guard_safe;
static uint32_t key_colors,key_font,key_display,key_cpu,key_table[320];
static int key_glyph(uint32_t id,uint32_t x,uint32_t foreground,uint32_t background,unsigned ch)
{
    uint32_t s;
    if(!record(id,&s)||!valid(s+12u,4))return 0;
    if(signed32(read_bytes(s+12u,4))<=signed32(x))return 1;
    if(!valid(s+16u,4))return 0;
    if(signed32(read_bytes(s+16u,4))<=10)return 1;
    for(unsigned row=0;row<16;++row){
        uint32_t f=key_font+ch*16u+row;if(!valid(f,1))return 0;
        unsigned bits=read_bytes(f,1);if(bits!=initial[index_of(f)])++key_font_reloads;
        for(unsigned column=0;column<8;++column){
            if(signed32(x+column)>=signed32(read_bytes(s+12u,4))||
               signed32(10u+row)>=signed32(read_bytes(s+16u,4)))continue;
            if(!valid(s,4))return 0;
            unsigned bytes=key_display==32u?4u:1u;
            uint32_t out=(uint32_t)((uint64_t)read_bytes(s,4)+
                ((uint64_t)(10u+row)*read_bytes(s+12u,4)+x+column)*bytes);
            if(!write_bytes(out,bits&(0x80u>>column)?foreground:background,bytes))return 0;
            ++key_text_stores;if(out-key_font<2048u)++key_font_stores;
        }
    }
    return 1;
}
static int key_model(uint32_t *result)
{
    uint32_t s,file=args[3],header=file+14u,palette=file+54u;
    if(!record(args[0],&s)||!valid(s+36u,4))return 0;
    if(read_bytes(s+36u,4)!=32u){*result=s;return 1;}
    if(!valid(header+14u,2))return 0;
    unsigned bits=read_bytes(header+14u,2),mode;
    if(bits!=24u&&bits!=8u){*result=header;return 1;}
    mode=bits==8u;uint32_t row=0,h,budget=0;
    for(;;){
        if(++budget>4096u){++key_unsafe_budget;return 0;}
        if(!valid(header+8u,4))return 0;
        h=read_bytes(header+8u,4);if(row>=h)break;
        for(uint32_t column=0;;++column){
            if(++budget>4096u){++key_unsafe_budget;return 0;}
            if(!valid(header+4u,4))return 0;
            uint32_t w=read_bytes(header+4u,4);if(column>=w)break;
            if(!valid(file+10u,4))return 0;
            uint32_t p=(uint32_t)((uint64_t)incoming(3)+read_bytes(file+10u,4)+
                ((uint64_t)w*row+column)*(mode?1u:3u)),color;
            if(!mode){if(!valid(p,4))return 0;color=read_bytes(p,4)%0x1000000u;}
            else{
                if(!valid(p,1))return 0;
                uint32_t entry=palette+read_bytes(p,1)*4u;
                if(!valid(entry,4))return 0;
                color=read_bytes(entry,4);
            }
            if(color==incoming(4)){++key_matches;continue;}
            if(!valid(header+8u,4))return 0;
            uint32_t yy=(uint32_t)((uint64_t)incoming(2)+read_bytes(header+8u,4)-row-1u);
            if(!pixel(incoming(0),incoming(1)+column,yy,color,4))return 0;
            ++key_pixels;
        }
        ++row;
    }
    ++key_paths[mode];
    if(mode){*result=h;return 1;}
    if(!valid(key_colors,4)||!valid(key_colors+56u,4))return 0;
    uint32_t fg=read_bytes(key_colors,4),bg=read_bytes(key_colors+56u,4),id=incoming(0);
    if(fg!=get32(initial,key_colors)||bg!=get32(initial,key_colors+56u))++key_color_changes;
    ++key_text_calls;
    if(!key_glyph(id,10,fg,bg,'t')||!key_glyph(id,18,fg,bg,'='))return 0;
    *result=0x83cf55u;return 1;
}
static int key_oracle(uint32_t *result)
{
    uint32_t *counters[]={&writes,&bitmap_stores,&metadata_stores,&caller_stores,&skipped_pixels,
        &key_paths[0],&key_paths[1],&key_matches,&key_pixels,&key_text_calls,&key_text_stores,
        &key_font_stores,&key_font_reloads,&key_color_changes};
    uint32_t saved[sizeof(counters)/sizeof(counters[0])];
    for(unsigned i=0;i<sizeof(counters)/sizeof(counters[0]);++i)saved[i]=*counters[i];
    if(key_model(result))return 1;
    for(unsigned i=0;i<sizeof(counters)/sizeof(counters[0]);++i)*counters[i]=saved[i];
    return 0;
}
static void key_setup(uint32_t width,uint32_t height,uint32_t depth,uint32_t bits,
    uint32_t iw,uint32_t ih,uint32_t destination)
{
    setup(width,height,depth,bits,iw,ih,destination);
    args[4]=random32();args[5]=random32();key_colors=DATA+0x2801u+alignment;
    key_font=DATA+0x3001u+alignment;
}
static uint32_t first_color(unsigned bits)
{
    uint32_t p=bitmap_base+0x1000u;
    return bits==24u?get32(initial,p)%0x1000000u:
        get32(initial,bitmap_base+54u+initial[index_of(p)]*4u);
}
static void key_exercise(void)
{
    scenario=cases+skipped;begin_oracle();uint32_t value;
    if(!key_oracle(&value)){++skipped;return;}
    for(implementation=0;implementation<3;++implementation){
        REQUIRE(load(implementation?"build/bitmap_key_candidate.x":"build/bitmap_key_original.x",0x800000),"key images");
        *(volatile uint32_t *)0x1b0e4=0x620000;*(volatile uint32_t *)0x1b034=0x610000;
        *(volatile uint32_t *)0x610024=key_display;*(volatile uint32_t *)0x1a868=key_cpu?1u<<25:0;
        *(volatile uint32_t *)0x1b0e0=key_colors;*(volatile uint32_t *)0x1b020=key_font;
        for(unsigned i=0;i<320;++i)*(volatile uint32_t *)(0x620000u+i*4u)=i*0x1234567u+cases;
        *(volatile uint32_t *)(0x620000u+6u*4u)=0x81db5a;
        ((void (*)(void))0x8005aa)();
        REQUIRE(*(volatile uint32_t *)(0x620000u+78u*4u)==0x821446u,"key initialized slot");
        for(unsigned i=0;i<320;++i){
            uint32_t entry=*(volatile uint32_t *)(0x620000u+i*4u);
            if(!implementation)key_table[i]=entry;else REQUIRE(entry==key_table[i],"key service table");
        }
        for(unsigned i=0;i<4096;++i)globals_initial[i]=*(volatile uint8_t *)(0x1b000u+i);
        reset();uint32_t actual[7];
        const void *fn=implementation==1u?(void *)kernel_bitmap_key_draw_821446:(void *)(uintptr_t)*(volatile uint32_t *)(0x620000u+78u*4u);
        REQUIRE(!raster_checked_call(fn,args,actual),"key ABI");compare(actual,value);
        for(unsigned i=0;i<320;++i)REQUIRE(*(volatile uint32_t *)(0x620000u+i*4u)==key_table[i],"key table mutation");
        for(unsigned i=0;i<4096;++i)REQUIRE(*(volatile uint8_t *)(0x1b000u+i)==globals_initial[i],"key globals");
    }
    for(unsigned i=0;i<6;++i)if(incoming(i)!=args[i])++key_changed_words;
    if(incoming(4)!=args[4])++key_changed_keys;
    if(incoming(3)!=args[3])++changed_bitmap;
    ++cases;
}
static void key_guards(void)
{
    stage=9;alignment=0;
    for(unsigned probe=0;probe<6;++probe){
        args[0]=0;args[1]=args[2]=0;
        key_setup(1,1,probe==5u?8u:32u,probe==4u?8u:24u,probe>=3u?0u:1u,probe>=3u?0u:1u,DATA+0x2000u);
        if(probe<3u)REQUIRE(put(initial,bitmap_base+10u,DATA+16384u-(probe+1u)-bitmap_base,4),"key DWORD fetch guard");
        if(probe==5u)args[3]=0xfffff000u;
        for(implementation=0;implementation<3;++implementation){
            REQUIRE(load(implementation?"build/bitmap_key_candidate.x":"build/bitmap_key_original.x",0x800000),"key guard image");
            reset();*(volatile uint32_t *)0x1b0e0=0xdead0000;
            int pid=syscall3(2,0,0,0);REQUIRE(pid>=0,"key guard fork");
            if(!pid){
                uint32_t actual[7];const void *fn=implementation==1u?(void *)kernel_bitmap_key_draw_821446:(void *)0x821446;
                raster_checked_call(fn,args,actual);
                syscall3(1,(probe==4u?actual[0]==0u:actual[0]==surface)?0u:1u,0,0);
            }
            uint32_t status=0;REQUIRE(syscall3(7,(uint32_t)pid,(uint32_t)&status,0)==pid,"key guard wait");
            if(probe>=4u){REQUIRE(!status,"key early return avoids image/color dereference");++key_guard_safe;}
            else{REQUIRE((status&127u)==11u,"key DWORD overread/empty-image diagnostic fault");++key_guard_faults;}
        }
    }
}
void _start(void)
{
    mappings();REQUIRE(map(0x1a000,8192)&&map(0x610000,4096)&&map(0x620000,4096),"key maps");
    static const uint32_t depths[]={0,8,15,16,24,32},bits[]={0,1,4,8,16,24,32,0xffff};
    key_display=32;stage=1;
    for(unsigned d=0;d<6;++d)for(unsigned b=0;b<8;++b)for(unsigned w=0;w<8;++w)
      for(unsigned h=0;h<8;++h)for(unsigned match=0;match<2;++match){
        alignment=(w+h)&3u;args[0]=ids[(w+h+b)%16u];args[1]=args[2]=0;
        key_setup(8,8,depths[d],bits[b],w,h,DATA+0x1801u+alignment);
        if(match)args[4]=first_color(bits[b]);
        key_exercise();
    }
    stage=2;
    for(unsigned mode=0;mode<2;++mode)for(alignment=0;alignment<4;++alignment)
      for(unsigned x=0;x<16;++x)for(unsigned y=0;y<16;++y){
        args[0]=ids[(x+y)%16u];args[1]=values[x];args[2]=values[y];
        key_setup(values[15u-x],values[15u-y],32,mode?8u:24u,3,3,DATA+0x1801u+alignment);key_exercise();
    }
    stage=3;
    for(unsigned mode=0;mode<2;++mode)for(unsigned w=0;w<16;++w)for(unsigned h=0;h<16;++h){
        alignment=(w+h)&3u;args[0]=0;args[1]=args[2]=0;
        key_setup(8,8,32,mode?8u:24u,values[w],values[h],DATA+0x1801u+alignment);key_exercise();
    }
    stage=4;
    for(unsigned mode=0;mode<2;++mode)for(alignment=0;alignment<4;++alignment)
      for(unsigned w=1;w<5;++w)for(unsigned h=1;h<5;++h)for(unsigned alias=0;alias<40;++alias){
        uint32_t s=DATA+0x101u+alignment,b=DATA+0x401u+alignment;
        uint32_t destination=alias<24u?ARGUMENTS+alias:alias<34u?s+(alias-24u)*4u:
            alias<36u?0x500000u+(alias-34u)*4u:alias<38u?b+10u+(alias-36u)*8u:
            alias==38u?b+54u:b+0x1000u;
        args[0]=0;args[1]=args[2]=0;
        key_setup(8,8,32,mode?8u:24u,w,h,destination);
        if((w+h)%2u)args[4]=first_color(mode?8u:24u);
        key_exercise();
    }
    stage=5;
    for(unsigned d=0;d<6;++d)for(key_cpu=0;key_cpu<2;++key_cpu)for(alignment=0;alignment<4;++alignment)
      for(unsigned alias=0;alias<5;++alias)for(unsigned match=0;match<2;++match){
        key_display=depths[d];unsigned bytes=key_display==32u?4u:1u;
        uint32_t destination=alias==0u?DATA+0x1801u+alignment:alias<3u?DATA+0x2801u+alignment+(alias-1u)*56u:
            alias==3u?DATA+0x3001u+alignment-(10u*32u+10u)*bytes:ARGUMENTS;
        args[0]=0;args[1]=args[2]=0;key_setup(32,26,32,24,1,1,destination);
        if(match)args[4]=first_color(24);
        key_exercise();
    }
    key_guards();
    REQUIRE(key_paths[0]&&key_paths[1]&&key_matches&&key_pixels&&key_text_calls&&key_text_stores&&key_font_stores&&key_font_reloads&&key_color_changes&&key_changed_keys,"key coverage");
    PRINT("Bitmap key scenarios/executions/excluded unsafe/budget: ");number(cases);PRINT(" ");number(cases*3u);PRINT(" ");number(skipped);PRINT(" ");number(key_unsafe_budget);
    PRINT("; RGB24/palette8/key matches/draws/text calls/text stores/font stores/font reloads/live text colors: ");number(key_paths[0]);PRINT(" ");number(key_paths[1]);PRINT(" ");number(key_matches);PRINT(" ");number(key_pixels);PRINT(" ");number(key_text_calls);PRINT(" ");number(key_text_stores);PRINT(" ");number(key_font_stores);PRINT(" ");number(key_font_reloads);PRINT(" ");number(key_color_changes);
    PRINT("; changed caller/key/bitmap arguments: ");number(key_changed_words);PRINT(" ");number(key_changed_keys);PRINT(" ");number(changed_bitmap);
    PRINT("; fault/safe probes: ");number(key_guard_faults);PRINT(" ");number(key_guard_safe);
    PRINT("; bitmap key verified with real pixel/formatter/glyph/initializer/slot 78/32 KiB memory/320 service words/caller/EAX/ABI; no helper substitutions\n");
    syscall3(1,0,0,0);
}
