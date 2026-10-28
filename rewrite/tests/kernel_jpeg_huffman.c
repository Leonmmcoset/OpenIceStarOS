#include <stdint.h>
#include "../kernel/jpeg_huffman.h"
#define CONTEXT 0x1000000u
#define SOURCE 0x1100000u
#define ARGUMENTS 0x640f60u
#define CONTEXT_BYTES 131072u
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
{const char *p=which?"build/jpeg_huffman_candidate.x":"build/jpeg_huffman_original.x";int fd=syscall3(5,(uint32_t)p,0,0);REQUIRE(fd>=0,"image open");int n=syscall3(3,fd,0x800000,0x40000);syscall3(6,fd,0,0);REQUIRE(n==250099,"image length");}
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
static uint32_t length_stores,code_stores,symbol_stores,extra_codes,zero_tables;
static uint32_t budget;
static int write_n(uint32_t a,uint32_t v,unsigned n)
{if(++budget>20000u||!put(expected,a,v,n))return 0;++stores;return 1;}
static int model(uint32_t *result)
{
    uint32_t p,end,a,b,remaining;budget=0;
    if(!cursor(&p)||!field(0x5be4u,&end))return 0;
    if(p+2u>end)goto truncated;
    if(!input(0,&a)||!input(1,&b))return 0;
    remaining=a*256u+b-2u;
    if(!cursor(&p)||!set_field(0x5be0u,p+2u))return 0;
    for(unsigned records=0;(int32_t)remaining>0;++records){
        if(records>=16u||!cursor(&p)||!field(0x5be4u,&end))return 0;
        if(p+17u>end)goto truncated;
        uint32_t c,flag,table,total=0,index=0;
        if(!input(0,&flag)||!context(&c))return 0;
        table=c+0x43c8u+(flag/16u)*3084u+(flag%16u)*1028u;
        for(unsigned length=1;length<=16u;++length){
            uint32_t count;if(!input(length,&count))return 0;total+=count;
            for(unsigned j=0;j<count;++j){if(!write_n(table+516u+index,length,1))return 0;++index;++length_stores;}
        }
        if(!write_value(table,total)||!cursor(&p)||!set_field(0x5be0u,p+17u))return 0;
        uint32_t size,code=0;index=0;
        if(!read_value(table+516u,1,&size))return 0;
        if(!total)++zero_tables;
        while((int32_t)index<(int32_t)total){
            if(!read_value(table+516u+index,1,&a))return 0;
            while(a==size){
                if(!write_n(table+4u+index*2u,code,2))return 0;
                if(index>=total)++extra_codes;
                ++code;++index;++code_stores;
                if(!read_value(table+516u+index,1,&a))return 0;
            }
            if((int32_t)index>=(int32_t)total)break;
            /* Skip missing tree levels one at a time. A length changed to
             * a smaller value produces the original very long wrap loop. */
            do{if(++budget>20000u)return 0;code+=code;++size;
                if(!read_value(table+516u+index,1,&a))return 0;
            }while(a!=size);
        }
        if(!cursor(&p)||!field(0x5be4u,&end))return 0;
        if(p+total>end)goto truncated;
        for(index=0;index<total;++index){
            if(!input(index,&a)||!write_n(table+772u+index,a,1))return 0;
            ++symbol_stores;
        }
        if(!cursor(&p)||!set_field(0x5be0u,p+total))return 0;
        remaining-=total+18u;++components;
    }
    *result=0;return 1;
truncated:
    if(!field(0x5be4u,&end)||!set_field(0x5be0u,end))return 0;
    *result=1;++failures;return 1;
}
static uint32_t profile_count(unsigned profile,unsigned length)
{
    if(profile==0u)return 0;
    if(profile==1u)return length==1u;
    if(profile==2u)return length==8u;
    if(profile==3u)return length==1u?2u:0u;
    if(profile==4u){static const uint8_t dc[16]={0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0};return dc[length-1u];}
    if(profile==5u)return 1;
    if(profile==6u)return length==16u?255u:0u;
    if(profile==7u)return length<=2u?255u:0u;
    if(profile==8u)return length<=2u?255u:length==3u?6u:0u;
    if(profile==9u)return length<=2u?255u:length==3u?3u:0u;
    if(profile==10u)return length==15u?255u:length==16u?1u:0u;
    return (length+profile)%5u;
}
static void setup(unsigned alignment,uint32_t source,uint32_t available,unsigned length,unsigned flag,unsigned profile)
{
    for(unsigned i=0;i<BYTES;++i)initial[i]=(uint8_t)(i*31u+(i/64u)*13u+cases*17u);
    args[0]=CONTEXT+0x101u+alignment;for(unsigned i=1;i<6;++i)args[i]=random32();
    REQUIRE(put(initial,args[0]+0x5be0u,source,4)&&put(initial,args[0]+0x5be4u,source+available,4),"cursor setup");
    REQUIRE(put(initial,source,length/256u,1)&&put(initial,source+1u,length%256u,1)&&put(initial,source+2u,flag,1),"Huffman header setup");
    for(unsigned i=1;i<=16u;++i)REQUIRE(put(initial,source+2u+i,profile_count(profile,i),1),"length counts setup");
}
static void exercise(void)
{
    scenario=cases+skipped;
    for(unsigned i=0;i<6;++i)REQUIRE(put(initial,ARGUMENTS+i*4u,args[i],4),"caller setup");
    for(unsigned i=0;i<BYTES;++i)expected[i]=initial[i];
    uint32_t result,saved[]={stores,components,failures,length_stores,code_stores,symbol_stores,extra_codes,zero_tables};
    if(!model(&result)){stores=saved[0];components=saved[1];failures=saved[2];length_stores=saved[3];code_stores=saved[4];symbol_stores=saved[5];extra_codes=saved[6];zero_tables=saved[7];++skipped;return;}
    for(implementation=0;implementation<3;++implementation){
        load(implementation);for(unsigned i=0;i<CONTEXT_BYTES+SOURCE_BYTES;++i)*(volatile uint8_t *)address(i)=initial[i];
        uint32_t actual[7];REQUIRE(!raster_checked_call(implementation==1u?(void *)kernel_jpeg_huffman_83ac7b:(void *)0x83ac7b,args,actual),"Huffman ABI");
        REQUIRE(actual[0]==result,"Huffman EAX");
        for(unsigned i=0;i<CONTEXT_BYTES+SOURCE_BYTES;++i)REQUIRE(*(volatile uint8_t *)address(i)==expected[i],"Huffman memory");
        for(unsigned i=0;i<6;++i){uint32_t v;REQUIRE(read_value(ARGUMENTS+i*4u,4,&v)&&actual[i+1u]==v,"Huffman incoming words");}
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
    for(unsigned probe=0;probe<6;++probe){
        setup(0,SOURCE,64u,20u,0,1);
        if(probe<=2u)REQUIRE(put(initial,args[0]+0x5be0u,probe==2u?0xfffffffeu:SOURCE+SOURCE_BYTES,4)&&
            put(initial,args[0]+0x5be4u,SOURCE+SOURCE_BYTES+(probe==1u?0u:2u),4),"guard cursor");
        if(probe==3u||probe==4u){
            /* Keep the table header and code area accessible while its
             * mandatory zero-count length read or run sentinel is not. */
            uint32_t table=CONTEXT+65536u-(probe==3u?4u:771u);
            args[0]=table-(0x43c8u+4u*0xc0cu);
            REQUIRE(put(initial,args[0]+0x5be0u,SOURCE,4)&&
                put(initial,args[0]+0x5be4u,SOURCE+8192u,4)&&
                put(initial,SOURCE+2u,0x40u,1),"table guard state");
            for(unsigned i=1;i<=16u;++i)REQUIRE(put(initial,SOURCE+2u+i,probe==4u&&i==1u?255u:0u,1),"guard counts");
        }
        if(probe==5u)setup(0,SOURCE,8192u,535u,0,8);
        for(implementation=0;implementation<3;++implementation){
            load(implementation);for(unsigned i=0;i<CONTEXT_BYTES+SOURCE_BYTES;++i)*(volatile uint8_t *)address(i)=initial[i];
            int pid=syscall3(2,0,0,0);REQUIRE(pid>=0,"guard fork");
            if(!pid){
                if(probe==3u||probe==4u)REQUIRE(syscall3(91,CONTEXT+65536u,65536u,0)==0,"unmap table guard");
                if(probe==5u)syscall3(27,1,0,0);
                uint32_t actual[7];raster_checked_call(implementation==1u?(void *)kernel_jpeg_huffman_83ac7b:(void *)0x83ac7b,args,actual);
                syscall3(1,actual[0]==1u?0:1,0,0);
            }
            uint32_t status=0;REQUIRE(syscall3(7,pid,(uint32_t)&status,0)==pid,"guard wait");
            if(probe==1u)REQUIRE(!status,"truncated input avoids read");
            else if(probe==5u)REQUIRE((status&127u)==14u,"overlapping code/length wrap loop");
            else REQUIRE((status&127u)==11u,"unchecked source/table/sentinel faults");
        }
    }
}
static void composed(void)
{
    stage=6;REQUIRE(map(SOURCE,65536)&&map(0x1200000,0x100000),"composition mappings");
    for(implementation=0;implementation<2;++implementation){
        load(implementation);int fd=syscall3(5,(uint32_t)"build/jpeg_huffman_fixture.jpg",0,0);REQUIRE(fd>=0,"JPEG open");
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
    static const unsigned lengths[]={0,1,2,3,17,18,19,20,31,35,273,274,529,535,1027,65535};
    static const unsigned limits[]={0,1,2,3,18,19,20,31,35,274,535,8192};
    stage=1;
    for(unsigned f=0;f<256;++f)for(unsigned p=0;p<12;++p){
        unsigned total=0;for(unsigned i=1;i<=16;++i)total+=profile_count(p,i);
        setup(f%4u,SOURCE+0x101u,8192u,19u+total,f,p);exercise();
    }
    stage=2;
    for(unsigned p=0;p<12;++p)for(unsigned n=0;n<16;++n)for(unsigned l=0;l<12;++l){
        setup((p+n+l)%4u,SOURCE+0x101u,limits[l],lengths[n],(p+n)%2u?0x10u:0u,p);exercise();
    }
    stage=3;
    static const unsigned offsets[]={0,1,4,16,0x43c8,0x45cc,0x46cc,0x47cc,0x4fd4,0x51d8,0x5bce,0x5bdf,0x5be0,0x5be4};
    for(unsigned o=0;o<14;++o)for(unsigned p=0;p<8;++p)for(unsigned a=0;a<4;++a){
        setup(a,CONTEXT+0x101u+a+offsets[o],1027,64,0,p);exercise();
    }
    stage=4;
    /* Equal slack lengths must produce code stores past the declared end. */
    for(unsigned length=1;length<=16u;++length)for(unsigned slack=0;slack<16;++slack){
        setup(length%4u,SOURCE+0x101u,8192u,20u,0,0);
        REQUIRE(put(initial,SOURCE+0x103u+length,1,1),"single length");
        uint32_t table=args[0]+0x43c8u;
        for(unsigned i=1;i<=slack;++i)REQUIRE(put(initial,table+516u+i,length,1),"length slack");
        REQUIRE(put(initial,table+517u+slack,0,1),"slack sentinel");exercise();
    }
    /* Multiple DHT records in one payload. */
    for(unsigned records=1;records<=8u;++records)for(unsigned a=0;a<4;++a){
        setup(a,SOURCE+0x101u,8192u,2u+records*18u,0,1);
        for(unsigned r=0;r<records;++r){
            uint32_t p=SOURCE+0x103u+r*18u;
            REQUIRE(put(initial,p,r%2u?0x10u:0u,1),"record flag");
            for(unsigned i=1;i<=16;++i)REQUIRE(put(initial,p+i,i==8u,1),"record counts");
            REQUIRE(put(initial,p+17u,r*31u,1),"record symbol");
        }
        exercise();
    }
    for(unsigned offset=0;offset<=16;++offset){
        setup(0,SOURCE,0,2,0,0);for(unsigned i=1;i<6;++i)args[i]=0;
        args[0]=ARGUMENTS+offset-0x5be0u;exercise();
    }
    guards();composed();REQUIRE(components&&failures&&stores&&source_changes&&extra_codes&&zero_tables,"Huffman coverage");
    PRINT("JPEG Huffman scenarios/executions/excluded: ");number(cases);PRINT(" ");number(cases*3u);PRINT(" ");number(skipped);
    PRINT("; records/truncations/stores/source changes/caller changes: ");number(components);PRINT(" ");number(failures);PRINT(" ");number(stores);PRINT(" ");number(source_changes);PRINT(" ");number(caller_changes);
    PRINT("; lengths/codes/symbols/extra codes/zero tables: ");number(length_stores);PRINT(" ");number(code_stores);PRINT(" ");number(symbol_stores);PRINT(" ");number(extra_codes);PRINT(" ");number(zero_tables);
    PRINT("; fault/loop/safe probes: 12 3 3; real JPEG composed executions: 2; jpeg huffman verified with bytewise state/source/caller/EAX/ABI and full real pixel/workspace comparisons; no helper substitutions\n");
    syscall3(1,0,0,0);
}
