#include <stdint.h>
#include "../kernel/jpeg_frame.h"
#define CONTEXT 0x1000000u
#define SOURCE 0x1100000u
#define ARGUMENTS 0x640f60u
#define CONTEXT_BYTES 65536u
#define SOURCE_BYTES 8192u
#define BYTES (CONTEXT_BYTES+SOURCE_BYTES+24u)
#define PRINT(s) syscall3(4,1,(uint32_t)(s),sizeof(s)-1u)
static uint8_t initial[BYTES],expected[BYTES],reference_pixels[0x100000];
static uint8_t reference_context[CONTEXT_BYTES];
static uint32_t args[6],stage,scenario,implementation,cases,skipped;
static uint32_t components,failures,stores,source_changes,caller_changes;
static uint32_t state=0xa1234719u;
extern uint32_t raster_checked_call(const void *,const uint32_t *,uint32_t *);
static int syscall3(int n,uint32_t a,uint32_t b,uint32_t c)
{int r;__asm__ volatile("int $0x80":"=a"(r):"a"(n),"b"(a),"c"(b),"d"(c):"memory");return r;}
static void number(uint32_t v)
{char d[10];unsigned n=10;do{d[--n]='0'+v%10u;v/=10u;}while(v);syscall3(4,1,(uint32_t)(d+n),10-n);}
static void fail(const char *s,uint32_t n)
{PRINT("FAIL: ");syscall3(4,1,(uint32_t)s,n);PRINT(" stage/scenario/implementation: ");number(stage);PRINT(" ");number(scenario);PRINT(" ");number(implementation);PRINT("\n");syscall3(1,1,0,0);}
#define REQUIRE(c,s) do{if(!(c))fail(s,sizeof(s)-1u);}while(0)
static int map(uint32_t a,uint32_t n)
{uint32_t p[]={a,n,7,0x32,0xffffffff,0};return (uint32_t)syscall3(90,(uint32_t)p,0,0)==a;}
static void load(unsigned which)
{const char *p=which?"build/jpeg_frame_candidate.x":"build/jpeg_frame_original.x";int fd=syscall3(5,(uint32_t)p,0,0);REQUIRE(fd>=0,"image open");int n=syscall3(3,fd,0x800000,0x40000);syscall3(6,fd,0,0);REQUIRE(n==250099,"image length");}
static uint32_t random32(void)
{state^=state<<13;state^=state>>17;state^=state<<5;return state;}
static uint32_t index_of(uint32_t a)
{if(a-CONTEXT<CONTEXT_BYTES)return a-CONTEXT;if(a-SOURCE<SOURCE_BYTES)return CONTEXT_BYTES+a-SOURCE;if(a-ARGUMENTS<24u)return CONTEXT_BYTES+SOURCE_BYTES+a-ARGUMENTS;return UINT32_MAX;}
static uint32_t address(unsigned i)
{return i<CONTEXT_BYTES?CONTEXT+i:i<CONTEXT_BYTES+SOURCE_BYTES?SOURCE+i-CONTEXT_BYTES:ARGUMENTS+i-CONTEXT_BYTES-SOURCE_BYTES;}
static int read_value(uint32_t a,unsigned n,uint32_t *v)
{*v=0;for(unsigned i=0;i<n;++i){uint32_t j=index_of(a+i);if(j==UINT32_MAX)return 0;*v|=(uint32_t)expected[j]<<(i*8u);}return 1;}
static int put(uint8_t *m,uint32_t a,uint32_t v,unsigned n)
{for(unsigned i=0;i<n;++i)if(index_of(a+i)==UINT32_MAX)return 0;for(unsigned i=0;i<n;++i)m[index_of(a+i)]=(uint8_t)(v>>(i*8u));return 1;}
static int write_value(uint32_t a,uint32_t v)
{if(!put(expected,a,v,4))return 0;++stores;return 1;}
/* Bytewise oracle with checked addresses. It keeps no native C pointers to
 * simulated structures and re-resolves the incoming context after stores. */
static int context(uint32_t *c){return read_value(ARGUMENTS,4,c);}
static int cursor(uint32_t *p)
{uint32_t c;return context(&c)&&read_value(c+0x5be0u,4,p);}
static int field(uint32_t off,uint32_t *v)
{uint32_t c;return context(&c)&&read_value(c+off,4,v);}
static int set_field(uint32_t off,uint32_t v)
{uint32_t c;return context(&c)&&write_value(c+off,v);}
static int input(uint32_t off,uint32_t *v)
{uint32_t p;return cursor(&p)&&read_value(p+off,1,v);}
static int model(uint32_t *result)
{
    uint32_t p,end,a,b,count,max;
    if(!cursor(&p)||!field(0x5be4u,&end))return 0;
    if(p+8u>end)goto truncated;
    if(!input(3,&a)||!input(4,&b)||!set_field(4,a*256u+b))return 0;
    if(!input(5,&a)||!input(6,&b)||!set_field(0,a*256u+b))return 0;
    if(!input(7,&a)||!set_field(24,a)||!field(24,&count))return 0;
    if(!cursor(&p)||!set_field(0x5be0u,p+8u)||!cursor(&p)||!field(0x5be4u,&end))return 0;
    if(p+count*3u>end)goto truncated;
    if(count>256u&&count<0x80000000u)return 0;
    for(uint32_t i=0;(int32_t)i<(int32_t)count;++i){
        if(!input(0,&a)||!set_field(0x1cu+i*4u,a)||!input(1,&b))return 0;
        if(!set_field(0x28u+i*4u,b)||!set_field(0x34u+i*4u,b/16u)||!field(0x34u+i*4u,&a))return 0;
        if(!set_field(0x40u+i*4u,b%16u)||!field(0x40u+i*4u,&b)||!field(16,&max))return 0;
        if((int32_t)max<(int32_t)a&&!set_field(16,a))return 0;
        if(!field(20,&max))return 0;
        if((int32_t)max<(int32_t)b&&!set_field(20,b))return 0;
        if(!input(2,&a)||!set_field(0x4cu+i*4u,a)||!cursor(&p)||!set_field(0x5be0u,p+3u))return 0;
        ++components;
    }
    *result=0;return 1;
truncated:
    if(!field(0x5be4u,&end)||!set_field(0x5be0u,end))return 0;
    *result=1;++failures;return 1;
}
static void setup(unsigned alignment,uint32_t source,uint32_t length,unsigned count,uint32_t max)
{
    for(unsigned i=0;i<BYTES;++i)initial[i]=(uint8_t)(i*31u+(i/64u)*13u+cases*17u);
    args[0]=CONTEXT+0x101u+alignment;for(unsigned i=1;i<6;++i)args[i]=random32();
    REQUIRE(put(initial,args[0]+0x5be0u,source,4)&&put(initial,args[0]+0x5be4u,source+length,4),"cursor setup");
    REQUIRE(put(initial,args[0]+16u,max,4)&&put(initial,args[0]+20u,max,4),"sampling setup");
    REQUIRE(put(initial,source+7u,count,1),"count setup");
}
static void exercise(void)
{
    scenario=cases+skipped;
    for(unsigned i=0;i<6;++i)REQUIRE(put(initial,ARGUMENTS+i*4u,args[i],4),"caller setup");
    for(unsigned i=0;i<BYTES;++i)expected[i]=initial[i];
    uint32_t result,saved[]={stores,components,failures};
    if(!model(&result)){stores=saved[0];components=saved[1];failures=saved[2];++skipped;return;}
    for(implementation=0;implementation<3;++implementation){
        load(implementation);for(unsigned i=0;i<CONTEXT_BYTES+SOURCE_BYTES;++i)*(volatile uint8_t *)address(i)=initial[i];
        uint32_t actual[7];REQUIRE(!raster_checked_call(implementation==1u?(void *)kernel_jpeg_frame_83a919:(void *)0x83a919,args,actual),"frame ABI");
        REQUIRE(actual[0]==result,"frame EAX");
        for(unsigned i=0;i<CONTEXT_BYTES+SOURCE_BYTES;++i)REQUIRE(*(volatile uint8_t *)address(i)==expected[i],"frame memory");
        for(unsigned i=0;i<6;++i){uint32_t v;REQUIRE(read_value(ARGUMENTS+i*4u,4,&v)&&actual[i+1u]==v,"frame incoming words");}
    }
    uint32_t original_source,final_source;
    REQUIRE(read_value(args[0]+0x5be0u,4,&final_source),"final source");
    original_source=0;for(unsigned i=0;i<4;++i)original_source|=(uint32_t)initial[index_of(args[0]+0x5be0u+i)]<<(i*8u);
    for(unsigned i=0;i<8;++i)if(index_of(original_source+i)!=UINT32_MAX&&expected[index_of(original_source+i)]!=initial[index_of(original_source+i)]){++source_changes;break;}
    for(unsigned i=0;i<24;++i)if(expected[CONTEXT_BYTES+SOURCE_BYTES+i]!=initial[CONTEXT_BYTES+SOURCE_BYTES+i]){++caller_changes;break;}
    (void)final_source;++cases;
}
static void guards(void)
{
    stage=5;
    for(unsigned probe=0;probe<3;++probe){
        setup(0,SOURCE,8u,0,0);
        REQUIRE(put(initial,args[0]+0x5be0u,SOURCE+SOURCE_BYTES,4)&&
            put(initial,args[0]+0x5be4u,SOURCE+SOURCE_BYTES+(probe==1u?0u:8u),4),"guard cursor");
        for(implementation=0;implementation<3;++implementation){
            load(implementation);for(unsigned i=0;i<CONTEXT_BYTES;++i)*(volatile uint8_t *)(CONTEXT+i)=initial[i];
            if(probe==2u)*(volatile uint32_t *)(args[0]+0x5be0u)=0xfffffffcu;
            int pid=syscall3(2,0,0,0);REQUIRE(pid>=0,"guard fork");
            if(!pid){uint32_t actual[7];raster_checked_call(implementation==1u?(void *)kernel_jpeg_frame_83a919:(void *)0x83a919,args,actual);syscall3(1,actual[0]==1u?0:1,0,0);}
            uint32_t status=0;REQUIRE(syscall3(7,pid,(uint32_t)&status,0)==pid,"guard wait");
            if(probe==1u)REQUIRE(!status,"truncated input avoids read");
            else REQUIRE((status&127u)==11u,"unchecked or wrapped source faults");
        }
    }
}
static void composed(void)
{
    stage=6;REQUIRE(map(SOURCE,65536)&&map(0x1200000,0x100000),"composition mappings");
    for(implementation=0;implementation<2;++implementation){
        load(implementation);int fd=syscall3(5,(uint32_t)"build/jpeg_frame_fixture.jpg",0,0);REQUIRE(fd>=0,"JPEG open");
        REQUIRE(syscall3(3,fd,SOURCE,65536)==JPEG_FILE_BYTES,"JPEG read");syscall3(6,fd,0,0);
        for(unsigned i=0;i<CONTEXT_BYTES;++i)*(volatile uint8_t *)(CONTEXT+i)=(uint8_t)(i*19u+71u);
        for(unsigned i=0;i<0x100000;++i)*(volatile uint8_t *)(0x1200000u+i)=0x6d;
        uint32_t call[]={CONTEXT,CONTEXT+0xf000u,JPEG_FILE_BYTES,SOURCE,0,0},actual[7];
        REQUIRE(!raster_checked_call((void *)0x83a70f,call,actual)&&actual[0]==1u,"real JPEG info");
        REQUIRE(*(volatile uint32_t *)(CONTEXT+0xf008u)==JPEG_WIDTH&&*(volatile uint32_t *)(CONTEXT+0xf00cu)==JPEG_HEIGHT,"real frame dimensions");
        call[1]=JPEG_FILE_BYTES;call[2]=SOURCE;call[3]=4;call[4]=0x1200000;
        REQUIRE(!raster_checked_call((void *)0x83a7a1,call,actual)&&!actual[0],"real JPEG decode");
        unsigned changed=0;
        for(unsigned i=0;i<0x100000;++i){uint8_t v=*(volatile uint8_t *)(0x1200000u+i);if(v!=0x6d)++changed;if(!implementation)reference_pixels[i]=v;else REQUIRE(v==reference_pixels[i],"real pixel/slack comparison");}
        REQUIRE(changed>JPEG_WIDTH*JPEG_HEIGHT,"real image output");
        for(unsigned i=0;i<CONTEXT_BYTES;++i){uint8_t v=*(volatile uint8_t *)(CONTEXT+i);if(!implementation)reference_context[i]=v;else REQUIRE(v==reference_context[i],"real workspace comparison");}
    }
}
void _start(void)
{
    REQUIRE(map(CONTEXT,CONTEXT_BYTES)&&map(SOURCE,SOURCE_BYTES)&&map(0x640000,4096)&&map(0x800000,0x40000),"mappings");
    static const unsigned counts[]={0,1,2,3,4,7,8,15,16,31,63,127,128,254,255};
    static const unsigned lengths[]={0,1,7,8,9,10,11,17,32,128,773};
    static const uint32_t maxima[]={0,1,15,16,0x7fffffff,0x80000000,0xffffffff};
    stage=1;
    for(unsigned a=0;a<4;++a)for(unsigned n=0;n<15;++n)for(unsigned l=0;l<11;++l){setup(a,SOURCE+0x101u,lengths[l],counts[n],maxima[(a+n+l)%7u]);exercise();}
    stage=2;
    for(unsigned s=0;s<256;++s)for(unsigned a=0;a<4;++a){setup(a,SOURCE+0x101u,17,3,maxima[(s+a)%7u]);for(unsigned i=0;i<3;++i)REQUIRE(put(initial,SOURCE+0x10au+i*3u,s,1),"sampling byte");exercise();}
    stage=3;
    static const unsigned offsets[]={0,1,4,8,16,20,24,28,40,52,64,76,128,1024,0x5bc0,0x5bdf,0x5be0,0x5be4};
    for(unsigned o=0;o<18;++o)for(unsigned n=0;n<8;++n)for(unsigned a=0;a<4;++a){setup(a,CONTEXT+0x101u+a+offsets[o],8u+counts[n]*3u,counts[n],0);exercise();}
    stage=4;
    for(unsigned a=0;a<4;++a)for(unsigned n=0;n<5;++n){setup(a,ARGUMENTS,24,counts[n],0);exercise();}
    /* A truncated cursor may itself occupy any incoming byte offset.
     * Offsets zero through three also overwrite the live context word. */
    for(unsigned offset=0;offset<=16;++offset){
        setup(0,SOURCE,0,0,0);
        for(unsigned i=1;i<6;++i)args[i]=0;
        args[0]=ARGUMENTS+offset-0x5be0u;
        exercise();
    }
    guards();composed();REQUIRE(components&&failures&&stores&&source_changes,"frame coverage");
    PRINT("JPEG frame scenarios/executions/excluded: ");number(cases);PRINT(" ");number(cases*3u);PRINT(" ");number(skipped);
    PRINT("; components/truncations/stores/source-alias changes/caller changes: ");number(components);PRINT(" ");number(failures);PRINT(" ");number(stores);PRINT(" ");number(source_changes);PRINT(" ");number(caller_changes);
    PRINT("; fault/safe probes: 6 3; real JPEG composed executions: 2; jpeg frame verified with bytewise state/source/caller/EAX/ABI and full real pixel/workspace comparisons; no helper substitutions\n");
    syscall3(1,0,0,0);
}
