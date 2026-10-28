/* Filesystem, drawing and free entry detours are explicit test boundaries.
 * Compare callback order/arguments, copied directory records, live caller
 * mutations, raw EAX, 32 KiB state, service tables and callee-saved ABI. */
#include "raster_fixture.h"
#include "../kernel/bitmap_files.h"
static uint32_t role,lookup_result,file_value,free_result,mutation,name_address,
    events[5][5],event_count,observed,record_pointer,record_reads,mutated_words,
    route_counts[2],draw_counts[2],early,reference_table[320],service_cases[12],
    real_mode,real_draws,real_fits,guard_faults;
static uint8_t global_snapshot[4096];
static uint8_t draw_prefix[5];
static uint8_t fit_prefix[5];
static void detour(uint32_t,uint32_t);
static uint32_t byte_at(uint8_t *memory,uint32_t address)
{
    uint32_t i=index_of(address);REQUIRE(i!=UINT32_MAX,"file oracle byte");return memory[i];
}
static uint32_t word_at(uint8_t *memory,unsigned word)
{return get32(memory,ARGUMENTS+word*4u);}
static void store(uint8_t *memory,uint32_t address,uint32_t value)
{
    if(memory)REQUIRE(put(memory,address,value,4),"file oracle store");
    else *(volatile uint32_t *)(uintptr_t)address=value;
}
static uint32_t current(uint8_t *memory,unsigned word)
{return memory?word_at(memory,word):*(volatile uint32_t *)(ARGUMENTS+word*4u);}
static void effect(unsigned kind,int model)
{
    uint8_t *m=model?expected:0;
    store(m,DATA+0x2800u+kind*4u,0xc5810429u+kind+mutation);
    if((kind==1u||kind==3u)&&(mutation&1u)){
        static const uint8_t replacement[]="D:\\CHANGED.BMP";
        for(unsigned i=0;i<sizeof(replacement);++i){
            if(model)REQUIRE(put(expected,name_address+i,replacement[i],1),"name mutation");
            else *(volatile uint8_t *)(name_address+i)=replacement[i];
        }
    }
    if(((kind==1u||kind==3u)&&(mutation&2u))||
       ((kind==2u||kind==4u)&&(mutation&4u))||
       ((kind==5u||kind==6u)&&(mutation&8u))){
        unsigned words=role?2u:4u;
        for(unsigned i=0;i<words;++i){
            uint32_t value=current(m,i)^((kind+i)*0x1234567u);
            store(m,ARGUMENTS+i*4u,value);if(model)++mutated_words;
        }
    }
    if(mutation&16u){
        store(m,DATA+0x10du,kind*3u);
        if(file_value-DATA<16380u)store(m,file_value,0x826d2940u+kind);
    }
}
static void event(unsigned kind,uint32_t a,uint32_t b,uint32_t c,uint32_t d,int model)
{
    uint32_t actual[]={kind,a,b,c,d};
    if(model){
        REQUIRE(event_count<5,"file event capacity");
        for(unsigned i=0;i<5;++i)events[event_count][i]=actual[i];
        ++event_count;
    }else{
        REQUIRE(observed<event_count,"unexpected file callback");
        for(unsigned i=0;i<5;++i)REQUIRE(events[observed][i]==actual[i],"file callback arguments/order");
        ++observed;
    }
    effect(kind,model);
}
uint32_t bitmap_lookup_absolute(uint32_t name,uint32_t record)
{
    REQUIRE(record-0x640000u<0xf40u&&record+32u<=ARGUMENTS,"directory stack record bounds");
    record_pointer=record;
    for(unsigned i=0;i<32;++i)*(volatile uint8_t *)(record+i)=(uint8_t)(i*17u+scenario);
    event(1,name,0,0,0,0);return lookup_result;
}
uint32_t bitmap_read_absolute(uint32_t record)
{
    REQUIRE(record==record_pointer,"same directory record");
    for(unsigned i=0;i<32;++i)REQUIRE(*(volatile uint8_t *)(record+i)==(uint8_t)(i*17u+scenario),"directory record contents");
    ++record_reads;event(2,0,0,0,0,0);return file_value;
}
uint32_t bitmap_lookup_relative(uint32_t name)
{event(3,name,0,0,0,0);return lookup_result;}
uint32_t bitmap_read_relative(uint32_t index)
{event(4,index,0,0,0,0);return file_value;}
uint32_t bitmap_draw(uint32_t id,uint32_t x,uint32_t y,uint32_t file)
{
    event(5,id,x,y,file,0);
    if(real_mode){
        for(unsigned i=0;i<5;++i)*(volatile uint8_t *)(0x81ffc4u+i)=draw_prefix[i];
        uint32_t value=((uint32_t (*)(uint32_t,uint32_t,uint32_t,uint32_t))0x81ffc4)(id,x,y,file);
        detour(0x81ffc4,(uint32_t)bitmap_draw);++real_draws;return value;
    }
    return 0xc2705391u;
}
uint32_t bitmap_fit(uint32_t id,uint32_t file)
{
    event(6,id,file,0,0,0);
    if(real_mode>=5u){
        for(unsigned i=0;i<5;++i)*(volatile uint8_t *)(0x8204acu+i)=fit_prefix[i];
        uint32_t value=((uint32_t (*)(uint32_t,uint32_t))0x8204ac)(id,file);
        detour(0x8204ac,(uint32_t)bitmap_fit);++real_fits;return value;
    }
    return 0xd26a3918u;
}
uint32_t bitmap_free(uint32_t file)
{event(7,file,0,0,0,0);return free_result;}
static void render_model(void)
{
    unsigned conversion=real_mode>=5u?(real_mode==6u?4u:1u):real_mode;
    uint32_t s=get32(expected,0x500000u+word_at(expected,0)*4u);
    uint32_t buffer=get32(expected,s),stride=get32(expected,s+12u),bytes=conversion==4u?1u:4u;
    uint32_t x=role?0u:word_at(expected,1),y=role?0u:word_at(expected,2);
    static const unsigned matrix[]={3,1,0,2};
    for(unsigned row=0;row<2;++row)for(unsigned column=0;column<3;column+=conversion==3u?2u:1u){
        uint32_t source=file_value+0x800u+((1u-row)*3u+column)*(conversion==1u||conversion==4u?3u:1u);
        unsigned count=conversion==3u?2u:1u;
        for(unsigned nibble=0;nibble<count;++nibble){
            uint32_t color;
            if(conversion==1u)color=get32(expected,source)&0xffffffu;
            else if(conversion==4u){
                unsigned threshold=matrix[row*2u+column%2u];
                color=16u+(byte_at(expected,source+2u)*21u/256u+threshold)/4u+
                    6u*((byte_at(expected,source+1u)*21u/256u+threshold)/4u)+
                    36u*((byte_at(expected,source)*21u/256u+threshold)/4u);
            }else{
                unsigned index=byte_at(expected,source);
                if(conversion==3u)index=nibble?index/16u:index%16u;
                color=get32(expected,file_value+54u+index*4u);
            }
            REQUIRE(put(expected,buffer+((y+row)*stride+x+column+nibble)*bytes,color,bytes),"real BMP composition store");
        }
    }
}
static uint32_t oracle(void)
{
    event_count=0;
    if(!role){
        if(signed32(args[1])<0){++early;return 0x6e25c813u;}
        uint32_t s=get32(expected,0x500000u+args[0]*4u),width=get32(expected,s+12u);
        if(signed32(args[1])>=signed32(width)||signed32(args[2])<0){++early;return width;}
        uint32_t height=get32(expected,s+16u);
        if(signed32(args[2])>=signed32(height)){++early;return height;}
    }
    uint32_t name=word_at(expected,role?1u:3u),first=byte_at(expected,name);
    int absolute=first>66u&&first<=70u&&byte_at(expected,name+1u)==58u&&byte_at(expected,name+2u)==92u;
    ++route_counts[absolute];
    event(absolute?1u:3u,name,0,0,0,1);
    if(absolute?signed32(lookup_result)<=0:signed32(lookup_result)<0)return lookup_result;
    event(absolute?2u:4u,absolute?0:lookup_result,0,0,0,1);
    if(!file_value)return 0;
    ++draw_counts[role];
    if(role){event(6,word_at(expected,0),file_value,0,0,1);if(real_mode)render_model();}
    else{
        event(5,word_at(expected,0),word_at(expected,1),word_at(expected,2),file_value,1);
        if(real_mode)render_model();
    }
    event(7,file_value,0,0,0,1);return free_result;
}
static void detour(uint32_t entry,uint32_t target)
{
    *(volatile uint8_t *)entry=0xe9;
    *(volatile uint32_t *)(entry+1u)=target-entry-5u;
}
static void setup(void)
{
    pattern();name_address=DATA+0x501u+(cases&3u);
    for(unsigned i=0;i<6;++i)args[i]=random32();
    args[0]=ids[cases%16u];args[1]=role?name_address:1;args[2]=2;args[3]=name_address;
    uint32_t s=DATA+0x101u;
    REQUIRE(put(initial,0x500000u+args[0]*4u,s,4)&&put(initial,s+12u,8,4)&&put(initial,s+16u,8,4),"file surface");
    static const char name[]="C:\\TEST.BMP";
    for(unsigned i=0;i<sizeof(name);++i)REQUIRE(put(initial,name_address+i,name[i],1),"file name");
    lookup_result=1;file_value=DATA+0x1801u;free_result=random32();mutation=0;
}
static void exercise(void)
{
    scenario=cases+skipped;begin_oracle();uint32_t result=oracle();
    static const uint32_t entries[]={0x81fec2,0x82040c},slots[]={74,140},depths[]={0,8,15,16,24,32};
    const void *native[]={(void *)kernel_bitmap_draw_file_81fec2,(void *)kernel_bitmap_fit_file_82040c};
    unsigned depth=cases%6u,cpu=cases/6u%2u;
    for(implementation=0;implementation<3;++implementation){
        REQUIRE(load(implementation?"build/bitmap_files_candidate.x":"build/bitmap_files_original.x",0x800000),"bitmap file images");
        *(volatile uint32_t *)0x1b0e4=0x620000;*(volatile uint32_t *)0x1b034=0x610000;
        *(volatile uint32_t *)0x610024=depths[depth];*(volatile uint32_t *)0x1a868=cpu?1u<<25:0;
        for(unsigned i=0;i<320;++i)*(volatile uint32_t *)(0x620000u+i*4u)=i*0x1234567u+cases;
        ((void (*)(void))0x8005aa)();
        REQUIRE(*(volatile uint32_t *)(0x620000u+slots[role]*4u)==entries[role],"file initialized service entry");
        for(unsigned i=0;i<320;++i){
            uint32_t value=*(volatile uint32_t *)(0x620000u+i*4u);
            if(!implementation)reference_table[i]=value;
            else REQUIRE(value==reference_table[i],"file entire service table");
        }
        for(unsigned i=0;i<4096;++i)global_snapshot[i]=*(volatile uint8_t *)(0x1b000u+i);
        detour(0x8263c0,(uint32_t)bitmap_lookup_absolute);detour(0x827454,(uint32_t)bitmap_read_absolute);
        detour(0x825c8e,(uint32_t)bitmap_lookup_relative);detour(0x827032,(uint32_t)bitmap_read_relative);
        for(unsigned i=0;i<5;++i){
            draw_prefix[i]=*(volatile uint8_t *)(0x81ffc4u+i);
            fit_prefix[i]=*(volatile uint8_t *)(0x8204acu+i);
        }
        detour(0x81ffc4,(uint32_t)bitmap_draw);detour(0x8204ac,(uint32_t)bitmap_fit);detour(0x815978,(uint32_t)bitmap_free);
        reset();observed=0;record_pointer=0;uint32_t actual[7];
        const void *fn=implementation==1?native[role]:(void *)(uintptr_t)*(volatile uint32_t *)(0x620000u+slots[role]*4u);
        REQUIRE(!raster_checked_call(fn,args,actual),"bitmap file ABI");compare(actual,result);
        REQUIRE(observed==event_count,"missing file callback");
        for(unsigned i=0;i<320;++i)REQUIRE(*(volatile uint32_t *)(0x620000u+i*4u)==reference_table[i],"file table mutation");
        for(unsigned i=0;i<4096;++i)REQUIRE(*(volatile uint8_t *)(0x1b000u+i)==global_snapshot[i],"file globals");
    }
    ++service_cases[depth*2u+cpu];++cases;
}
static void read_guards(void)
{
    /* Short-circuit lookahead: a rejected signed first byte must not read
     * past the page; accepted drives must read ':' and then backslash. */
    stage=5;real_mode=0;
    for(role=0;role<2;++role)for(unsigned probe=0;probe<3;++probe){
        setup();uint32_t p=DATA+16384u-(probe==2u?2u:1u);
        REQUIRE(put(initial,p,probe?'C':'B',1),"guard drive");
        if(probe==2u)REQUIRE(put(initial,p+1u,':',1),"guard colon");
        args[role?1u:3u]=p;lookup_result=0xffffffffu;
        if(!probe){exercise();continue;}
        for(implementation=0;implementation<3;++implementation){
            REQUIRE(load(implementation?"build/bitmap_files_candidate.x":"build/bitmap_files_original.x",0x800000),"file guard image");
            reset();int pid=syscall3(2,0,0,0);REQUIRE(pid>=0,"file guard fork");
            if(!pid){
                uint32_t actual[7];const void *native[]={(void *)kernel_bitmap_draw_file_81fec2,(void *)kernel_bitmap_fit_file_82040c};
                const void *fn=implementation==1u?native[role]:(void *)(uintptr_t)(role?0x82040c:0x81fec2);
                raster_checked_call(fn,args,actual);syscall3(1,0,0,0);
            }
            uint32_t status=0;REQUIRE(syscall3(7,(uint32_t)pid,(uint32_t)&status,0)==pid,"file guard wait");
            REQUIRE((status&127u)==11u,"unchecked absolute path lookahead fault");++guard_faults;
        }
    }
}
void _start(void)
{
    mappings();REQUIRE(map(0x1a000,8192)&&map(0x610000,4096)&&map(0x620000,4096),"file maps");
    stage=1;
    for(role=0;role<2;++role)for(unsigned position=0;position<3;++position)for(unsigned byte=0;byte<256;++byte){
        setup();REQUIRE(put(initial,name_address+position,byte,1),"path sweep");exercise();
    }
    stage=2;
    for(role=0;role<2;++role)for(unsigned route=0;route<3;++route)
      for(unsigned lookup=0;lookup<16;++lookup)for(unsigned buffer=0;buffer<8;++buffer)
      for(mutation=0;mutation<32;++mutation){
        unsigned saved=mutation;setup();mutation=saved;
        REQUIRE(put(initial,name_address,route==0u?'C':route==1u?'F':'B',1),"route prefix");
        lookup_result=values[lookup];file_value=values[buffer];exercise();
    }
    stage=3;role=0;
    for(unsigned x=0;x<16;++x)for(unsigned y=0;y<16;++y)
      for(unsigned dimension=0;dimension<16;++dimension){
        setup();args[1]=values[x];args[2]=values[y];
        REQUIRE(put(initial,DATA+0x10du,values[dimension],4)&&put(initial,DATA+0x111u,values[15u-dimension],4),"file geometry");
        exercise();
    }
    stage=4;role=0;
    for(real_mode=1;real_mode<=4;++real_mode)for(unsigned route=0;route<2;++route)for(unsigned seed=0;seed<12;++seed){
        setup();REQUIRE(put(initial,name_address,route?'C':'B',1),"real drawing route");
        REQUIRE(put(initial,DATA+0x101u,DATA+0x3001u,4)&&
            put(initial,DATA+0x125u,real_mode==4u?8u:32u,4),"real drawing surface");
        REQUIRE(put(initial,file_value+10u,0x800u,4)&&put(initial,file_value+18u,3,4)&&
            put(initial,file_value+22u,2,4)&&put(initial,file_value+28u,real_mode==2u?8u:real_mode==3u?4u:24u,2),"real drawing BMP");
        exercise();
    }
    role=1;
    for(real_mode=5;real_mode<=6;++real_mode)for(unsigned route=0;route<2;++route)for(unsigned seed=0;seed<12;++seed){
        setup();REQUIRE(put(initial,name_address,route?'C':'B',1),"real fit route");
        REQUIRE(put(initial,DATA+0x101u,DATA+0x3001u,4)&&put(initial,DATA+0x10du,3,4)&&
            put(initial,DATA+0x111u,2,4)&&put(initial,DATA+0x125u,real_mode==6u?8u:32u,4),"real fit surface");
        REQUIRE(put(initial,file_value+10u,0x800u,4)&&put(initial,file_value+18u,3,4)&&
            put(initial,file_value+22u,2,4)&&put(initial,file_value+28u,24,2),"real fit BMP");
        exercise();
    }
    read_guards();
    REQUIRE(route_counts[0]&&route_counts[1]&&draw_counts[0]&&draw_counts[1]&&early&&mutated_words&&record_reads,"file coverage");
    for(unsigned i=0;i<12;++i)REQUIRE(service_cases[i],"six depths/two CPU coverage");
    PRINT("Bitmap file scenarios/executions: ");number(cases);PRINT(" ");number(cases*3u);
    PRINT("; relative/absolute/draw/fit/early/mutated caller/record reads: ");number(route_counts[0]);PRINT(" ");number(route_counts[1]);PRINT(" ");number(draw_counts[0]);PRINT(" ");number(draw_counts[1]);PRINT(" ");number(early);PRINT(" ");number(mutated_words);PRINT(" ");number(record_reads);
    PRINT("; real renderer/fit executions/lookahead guard faults: ");number(real_draws);PRINT(" ");number(real_fits);PRINT(" ");number(guard_faults);
    REQUIRE(real_draws==288u&&real_fits==144u&&guard_faults==12u,"real renderer/fit/guard coverage");
    PRINT("; bitmap file wrappers verified against event/record/32 KiB memory/caller/EAX/ABI/initialized-service oracle; seven explicit helper entry detours\n");
    syscall3(1,0,0,0);
}
