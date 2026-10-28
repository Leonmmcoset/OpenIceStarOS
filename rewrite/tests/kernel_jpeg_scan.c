#include <stdint.h>
#include "../kernel/jpeg_scan.h"
#define CONTEXT 0x1000000u
#define SOURCE 0x1100000u
#define ARGUMENTS 0x640f60u
#define CONTEXT_BYTES 131072u
#define SOURCE_BYTES 8192u
#define BYTES (CONTEXT_BYTES+SOURCE_BYTES)
#define SHARED 0x1400000u
#define SNAPSHOT (BYTES+32u)
#define PRINT(s) syscall3(4,1,(uint32_t)(s),sizeof(s)-1u)
#define D(a) (*(volatile uint32_t *)(uintptr_t)(a))
static uint8_t initial[BYTES],reference[SNAPSHOT],reference_pixels[0x100000];
static uint8_t reference_context[CONTEXT_BYTES];
static uint32_t args[6],stage,scenario,implementation,cases,kind;
static uint32_t per_kind[3],faults,loops,safe_probes,caller_changes;
static uint32_t state=0x13a984c7u;
extern uint32_t raster_checked_call(const void *,const uint32_t *,uint32_t *);
static int syscall3(int n,uint32_t a,uint32_t b,uint32_t c)
{int r;__asm__ volatile("int $0x80":"=a"(r):"a"(n),"b"(a),"c"(b),"d"(c):"memory");return r;}
static void number(uint32_t v)
{char d[10];unsigned n=10;do{d[--n]='0'+v%10u;v/=10u;}while(v);syscall3(4,1,(uint32_t)(d+n),10-n);}
static void fail(const char *s,uint32_t n)
{PRINT("FAIL: ");syscall3(4,1,(uint32_t)s,n);PRINT(" stage/scenario/implementation: ");number(stage);PRINT(" ");number(scenario);PRINT(" ");number(implementation);PRINT("\n");syscall3(1,1,0,0);}
#define REQUIRE(c,s) do{if(!(c))fail(s,sizeof(s)-1u);}while(0)
static int map(uint32_t a,uint32_t n,unsigned shared)
{uint32_t p[]={a,n,7,shared?0x31u:0x32u,0xffffffff,0};return (uint32_t)syscall3(90,(uint32_t)p,0,0)==a;}
static void load(unsigned which)
{const char *p=which?"build/jpeg_scan_candidate.x":"build/jpeg_scan_original.x";int fd=syscall3(5,(uint32_t)p,0,0);REQUIRE(fd>=0,"image open");int n=syscall3(3,fd,0x800000,0x40000);syscall3(6,fd,0,0);REQUIRE(n==250099,"image length");}
static uint32_t random32(void)
{state^=state<<13;state^=state>>17;state^=state<<5;return state;}
static uint32_t address(unsigned i)
{return i<CONTEXT_BYTES?CONTEXT+i:SOURCE+i-CONTEXT_BYTES;}
static uint32_t index_of(uint32_t a)
{if(a-CONTEXT<CONTEXT_BYTES)return a-CONTEXT;if(a-SOURCE<SOURCE_BYTES)return CONTEXT_BYTES+a-SOURCE;return UINT32_MAX;}
static void put(uint32_t a,uint32_t v,unsigned n)
{for(unsigned i=0;i<n;++i){uint32_t j=index_of(a+i);REQUIRE(j!=UINT32_MAX,"setup address");initial[j]=(uint8_t)(v>>(i*8u));}}
static void setup(unsigned alignment)
{
    for(unsigned i=0;i<BYTES;++i)initial[i]=(uint8_t)(i*31u+(i/64u)*13u+cases*17u);
    args[0]=CONTEXT+0x101u+alignment;for(unsigned i=1;i<6;++i)args[i]=random32();
    put(args[0]+0x5be0u,SOURCE+0x101u,4);put(args[0]+0x5be4u,SOURCE+0x301u,4);
}
static void capture(void)
{
    const void *compiled[]={kernel_jpeg_scan_83af27,kernel_jpeg_prepare_83b233,kernel_jpeg_symbol_83b351};
    const uint32_t original[]={0x83af27,0x83b233,0x83b351};
    uint32_t actual[7];uint32_t abi=raster_checked_call(implementation==1u?compiled[kind]:(void *)original[kind],args,actual);
    for(unsigned i=0;i<BYTES;++i)*(volatile uint8_t *)(SHARED+i)=*(volatile uint8_t *)address(i);
    for(unsigned i=0;i<7;++i)D(SHARED+BYTES+i*4u)=actual[i];
    D(SHARED+BYTES+28u)=abi;
}
static void exercise(unsigned forked)
{
    scenario=cases;uint32_t first_status=0;
    for(implementation=0;implementation<3;++implementation){
        load(implementation);for(unsigned i=0;i<BYTES;++i)*(volatile uint8_t *)address(i)=initial[i];
        uint32_t status=0;
        if(forked){
            int pid=syscall3(2,0,0,0);REQUIRE(pid>=0,"probe fork");
            if(!pid){syscall3(27,1,0,0);capture();syscall3(1,0,0,0);}
            REQUIRE(syscall3(7,pid,(uint32_t)&status,0)==pid,"probe wait");
        }else capture();
        if(!implementation)first_status=status;
        REQUIRE(status==first_status,"original/C probe termination differs");
        if(status){REQUIRE((status&127u)==11u||(status&127u)==14u,"expected fault or bounded loop");continue;}
        REQUIRE(!D(SHARED+BYTES+28u),"preserved registers/stack/canaries");
        for(unsigned i=0;i<SNAPSHOT;++i){uint8_t v=*(volatile uint8_t *)(SHARED+i);if(!implementation)reference[i]=v;else REQUIRE(v==reference[i],"state/source/caller/EAX comparison");}
    }
    if(first_status){if((first_status&127u)==11u)faults+=3u;else loops+=3u;}
    else{
        if(forked)safe_probes+=3u;
        for(unsigned i=0;i<6;++i)if(*(uint32_t *)(reference+BYTES+4u+i*4u)!=args[i]){++caller_changes;break;}
    }
    ++per_kind[kind];++cases;
}
static void markers(void)
{
    kind=0;stage=1;
    /* Every marker byte; recognized helpers receive empty valid payloads. */
    for(unsigned m=0;m<256;++m)for(unsigned a=0;a<4;++a){
        setup(a);uint32_t p=SOURCE+0x101u;
        for(unsigned i=0;i<256;++i)put(p+i,0,1);
        put(p,255,1);put(p+1,m,1);put(p+2,0,1);put(p+3,2,1);
        put(p+4,0,1); /* zero components for SOF/SOS; empty DHT/DQT */
        put(args[0]+0x5be4u,p+32u,4);exercise(0);
    }
    stage=2;
    static const unsigned counts[]={0,1,2,3,4,7,16,31,127,255};
    static const unsigned limits[]={0,1,2,3,4,5,6,7,8,9,10,15,16,32,520};
    for(unsigned n=0;n<10;++n)for(unsigned l=0;l<15;++l)for(unsigned a=0;a<4;++a){
        setup(a);uint32_t p=SOURCE+0x101u;put(p,0xdaffu,2);put(p+2,0xffffu,2);put(p+4,counts[n],1);
        put(args[0]+0x5be4u,p+limits[l],4);exercise(0);
    }
    stage=3;
    /* SOI, ignored segment, restart interval and SOS in one stream. */
    for(unsigned n=0;n<256;++n){
        setup(n&3u);uint32_t p=SOURCE+0x101u;
        put(p,0xd8ffu,2);put(p+2,0xe0ffu,2);put(p+4,0x0600u,2);
        put(p+10,0xddffu,2);put(p+12,0,2);put(p+14,n*257u,2);
        put(p+16,0xdaffu,2);put(p+18,0,2);put(p+20,3,1);
        for(unsigned j=0;j<3;++j){put(p+21+j*2u,j+1,1);put(p+22+j*2u,n,1);}
        put(args[0]+0x5be4u,p+27u,4);exercise(0);
        REQUIRE(*(uint32_t *)(reference+args[0]-CONTEXT+0x5be0u)==p+30u,"SOS skips unchecked trailing bytes");
        REQUIRE(*(uint32_t *)(reference+BYTES)==0,"valid SOS result");
    }
}
static void preparation(void)
{
    kind=1;stage=4;
    static const uint32_t counts[]={0,1,2,3,4,7,16,31,255,0x7fffffffu,0x80000000u,0xffffffffu};
    for(unsigned n=0;n<12;++n)for(unsigned j=0;j<12;++j)for(unsigned a=0;a<4;++a){
        /* INT_MAX iterations are probed separately under an alarm. */
        if(n==9||j==9)continue;
        setup(a);put(args[0]+0x58u,counts[n],4);put(args[0]+0x18u,counts[j],4);
        put(args[0]+0x10u,random32(),4);put(args[0]+0x14u,random32(),4);
        for(unsigned i=0;i<8;++i){put(args[0]+0x1cu+i*4u,i%3u,4);put(args[0]+0x5cu+i*4u,i%4u,4);}
        /* The overlapping arrays above also exercise retained/duplicate IDs. */
        exercise(0);
    }
}
static void symbols(void)
{
    kind=2;stage=5;
    /* A single canonical code at each possible bit length; then miss it.
     * Bytes after the final entry are sentinels, independent of table count. */
    for(unsigned length=1;length<=16;++length)for(unsigned a=0;a<4;++a)for(unsigned miss=0;miss<2;++miss){
        setup(a);args[1]=a&1u;args[2]=a%3u;uint32_t t=args[0]+0x43c8u+args[1]*0xc0cu+args[2]*0x404u;
        put(t,0,4);put(t+4,miss?1u:0u,2);put(t+0x204u,length,1);put(t+0x205u,0,1);put(t+0x304u,0xa5u,1);
        put(args[0]+0x5becu,32,4);put(args[0]+0x5be8u,0,4);
        for(unsigned i=0;i<32;++i)put(SOURCE+0x101u+i,0,1);
        exercise(0);REQUIRE(*(uint32_t *)(reference+BYTES)==(miss?UINT32_MAX:0xa5u),"canonical code hit/miss");
    }
    /* All symbol values, bit refill and byte stuffing, arbitrary selectors. */
    for(unsigned s=0;s<256;++s)for(unsigned mode=0;mode<4;++mode){
        setup(mode);args[1]=(s>>4u);args[2]=s&15u;
        uint32_t t=args[0]+0x43c8u+args[1]*0xc0cu+args[2]*0x404u;
        put(t+0x204u,1,1);put(t+0x205u,0,1);put(t+4,mode==1u?1u:0u,2);put(t+0x304u,s,1);
        put(args[0]+0x5becu,mode==3u?32u:0u,4);put(args[0]+0x5be8u,mode==3u?0x80000000u:0u,4);
        for(unsigned i=0;i<32;++i)put(SOURCE+0x101u+i,mode==1u?255u:0u,1);
        if(mode==2u)put(args[0]+0x5be4u,SOURCE+0x101u,4);
        exercise(0);
    }
    /* Exhaust all positions of a complete 8-bit canonical codebook. The
     * declared count is deliberately zero: lookup uses length sentinels. */
    for(unsigned s=0;s<256;++s){
        setup(s&3u);args[1]=args[2]=0;uint32_t t=args[0]+0x43c8u;
        put(t,0,4);
        for(unsigned i=0;i<256;++i){put(t+4u+i*2u,i,2);put(t+0x204u+i,8,1);put(t+0x304u+i,255u-i,1);}
        put(args[0]+0x5becu,32,4);put(args[0]+0x5be8u,s<<24u,4);
        exercise(0);REQUIRE(*(uint32_t *)(reference+BYTES)==255u-s,"complete canonical codebook");
    }
}
static void probes(void)
{
    stage=6;
    for(unsigned k=0;k<3;++k)for(unsigned a=0;a<4;++a){
        kind=k;setup(a);
        if(k==0){put(args[0]+0x5be0u,a<2u?SOURCE+SOURCE_BYTES:0xfffffffeu,4);put(args[0]+0x5be4u,a==1u?SOURCE+SOURCE_BYTES:UINT32_MAX,4);}
        if(k==1){put(args[0]+0x58u,0x7fffffffu,4);put(args[0]+0x18u,0,4);}
        if(k==2){args[1]=0x10000000u;put(args[0]+0x5becu,32,4);}
        exercise(1);
    }
    /* Context/source aliases and caller-word destinations run in forks because
     * the original may overwrite a live pointer or loop on a zero length. */
    kind=0;
    static const unsigned offsets[]={0,4,16,20,0x58,0x5c,0x68,0x74,0xa4,0x40b8,0x43c4,0x5be0,0x5be4};
    for(unsigned o=0;o<13;++o){
        setup(o&3u);uint32_t p=args[0]+offsets[o];put(p,0xdaffu,2);put(p+2,0,2);put(p+4,3,1);
        put(args[0]+0x5be0u,p,4);put(args[0]+0x5be4u,p+32u,4);exercise(1);
    }
    for(unsigned o=0;o<6;++o){setup(0);args[0]=ARGUMENTS+o*4u;exercise(1);}
}
static void composed(void)
{
    stage=7;REQUIRE(map(SOURCE,65536,0)&&map(0x1200000,0x100000,0),"composition mappings");
    for(implementation=0;implementation<2;++implementation){
        load(implementation);int fd=syscall3(5,(uint32_t)"build/jpeg_scan_fixture.jpg",0,0);REQUIRE(fd>=0,"JPEG open");
        REQUIRE(syscall3(3,fd,SOURCE,65536)==JPEG_FILE_BYTES,"JPEG read");syscall3(6,fd,0,0);
        for(unsigned i=0;i<CONTEXT_BYTES;++i)*(volatile uint8_t *)(CONTEXT+i)=(uint8_t)(i*19u+71u);
        for(unsigned i=0;i<0x100000;++i)*(volatile uint8_t *)(0x1200000u+i)=0x6d;
        uint32_t call[]={CONTEXT,CONTEXT+0xf000u,JPEG_FILE_BYTES,SOURCE,0,0},actual[7];
        REQUIRE(!raster_checked_call((void *)0x83a70f,call,actual)&&actual[0]==1u,"real JPEG info");
        REQUIRE(D(CONTEXT+0xf008u)==JPEG_WIDTH&&D(CONTEXT+0xf00cu)==JPEG_HEIGHT,"real dimensions");
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
    REQUIRE(map(CONTEXT,CONTEXT_BYTES,0)&&map(SOURCE,SOURCE_BYTES,0)&&map(0x640000,4096,0)&&map(0x800000,0x40000,0)&&map(SHARED,0x23000,1),"mappings");
    markers();preparation();symbols();probes();composed();
    REQUIRE(faults&&safe_probes&&per_kind[0]&&per_kind[1]&&per_kind[2],"coverage");
    PRINT("JPEG scan scenarios/executions: ");number(cases);PRINT(" ");number(cases*3u);
    PRINT("; dispatcher/prepare/symbol scenarios: ");for(unsigned i=0;i<3;++i){number(per_kind[i]);PRINT(" ");}
    PRINT("; fault/loop/safe probe executions/caller changes: ");number(faults);PRINT(" ");number(loops);PRINT(" ");number(safe_probes);PRINT(" ");number(caller_changes);
    PRINT("; real JPEG info/decode executions: 2; jpeg scan verified with full state/source/caller/EAX/ABI and real pixel/workspace comparisons; no helper substitutions\n");
    syscall3(1,0,0,0);
}
