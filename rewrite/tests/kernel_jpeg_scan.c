#include <stdint.h>
#include "../kernel/jpeg_scan.h"
#include "../kernel/jpeg_basis.h"
#include "../kernel/jpeg_transform.h"
#include "../kernel/jpeg_block.h"
#include "../kernel/jpeg_output.h"
#include "../kernel/jpeg_decode.h"
#include "../build/jpeg_corpus.h"
#define CONTEXT 0x1000000u
#define SOURCE 0x1100000u
#define ARGUMENTS 0x640f60u
#define CONTEXT_BYTES 131072u
#define SOURCE_BYTES 8192u
#define BYTES (CONTEXT_BYTES+SOURCE_BYTES)
#define SHARED 0x1400000u
#define SNAPSHOT (BYTES+40u)
#define OUTPUT 0x2000000u
#define OUTPUT_BYTES (12u*1024u*1024u)
#define PRINT(s) syscall3(4,1,(uint32_t)(s),sizeof(s)-1u)
#define D(a) (*(volatile uint32_t *)(uintptr_t)(a))
static uint8_t initial[BYTES],reference[SNAPSHOT],reference_pixels[OUTPUT_BYTES];
static uint8_t reference_context[CONTEXT_BYTES];
static uint32_t args[8],stage,scenario,implementation,cases,kind;
static uint32_t per_kind[13],faults,loops,safe_probes,caller_changes,real_decodes;
static uint8_t basis_oracle[16384];
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
    args[0]=CONTEXT+0x101u+alignment;for(unsigned i=1;i<8;++i)args[i]=random32();
    put(args[0]+0x5be0u,SOURCE+0x101u,4);put(args[0]+0x5be4u,SOURCE+0x301u,4);
}
static void capture(void)
{
    const void *compiled[]={kernel_jpeg_scan_83af27,kernel_jpeg_prepare_83b233,kernel_jpeg_symbol_83b351,kernel_jpeg_basis_83b41b,kernel_jpeg_transform_83b653,kernel_jpeg_magnitude_83b73f,kernel_jpeg_block_83b7ab,kernel_jpeg_expand_83b96d,kernel_jpeg_color_83bc15,kernel_jpeg_mcu_83ba4f,kernel_jpeg_image_83be59,kernel_jpeg_info_83a70f,kernel_jpeg_decode_83a7a1};
    const uint32_t original[]={0x83af27,0x83b233,0x83b351,0x83b41b,0x83b653,0x83b73f,0x83b7ab,0x83b96d,0x83bc15,0x83ba4f,0x83be59,0x83a70f,0x83a7a1};
    uint32_t actual[9];uint32_t abi=raster_checked_call(implementation==1u?compiled[kind]:(void *)original[kind],args,actual);
    for(unsigned i=0;i<BYTES;++i)*(volatile uint8_t *)(SHARED+i)=*(volatile uint8_t *)address(i);
    for(unsigned i=0;i<9;++i)D(SHARED+BYTES+i*4u)=actual[i];
    D(SHARED+BYTES+36u)=abi;
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
        REQUIRE(!D(SHARED+BYTES+36u),"preserved registers/stack/canaries");
        for(unsigned i=0;i<SNAPSHOT;++i){uint8_t v=*(volatile uint8_t *)(SHARED+i);if(!implementation)reference[i]=v;else if(v!=reference[i]){PRINT("first difference offset/expected/actual: ");number(i);PRINT(" ");number(reference[i]);PRINT(" ");number(v);PRINT("\n");REQUIRE(0,"state/source/caller/EAX comparison");}}
    }
    if(first_status){if((first_status&127u)==11u)faults+=3u;else loops+=3u;}
    else{
        if(forked)safe_probes+=3u;
        for(unsigned i=0;i<8;++i)if(*(uint32_t *)(reference+BYTES+4u+i*4u)!=args[i]){++caller_changes;break;}
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
static void basis(void)
{
    stage=8;kind=3;
    int fd=syscall3(5,(uint32_t)"build/jpeg_basis_oracle.bin",0,0);REQUIRE(fd>=0,"basis oracle open");
    REQUIRE(syscall3(3,fd,(uint32_t)basis_oracle,sizeof(basis_oracle))==sizeof(basis_oracle),"basis oracle read");syscall3(6,fd,0,0);
    for(unsigned a=0;a<16;++a){
        setup(a);exercise(0);
        for(unsigned i=0;i<sizeof(basis_oracle);++i)REQUIRE(reference[args[0]-CONTEXT+i]==basis_oracle[i],"mathematical cosine basis oracle");
    }
    for(unsigned a=0;a<4;++a){
        setup(a);args[0]=CONTEXT+CONTEXT_BYTES-16384u+a;exercise(1);
        setup(a);args[0]=ARGUMENTS+a*4u;exercise(1);
    }
}
static void transform(void)
{
    kind=4;stage=9;
    static const uint32_t values[]={0,1,1024,0xfffffc00u,0x7fffffffu,0x80000000u,UINT32_MAX};
    for(unsigned f=0;f<64;++f)for(unsigned v=0;v<7;++v){
        setup(f&3u);args[1]=CONTEXT+0x2001u+(f&3u);args[2]=CONTEXT+0x8000u;
        for(unsigned i=0;i<64;++i)put(args[0]+i*4u,i==f?values[v]:0,4);
        for(unsigned i=0;i<sizeof(basis_oracle);++i)put(args[2]+i,basis_oracle[i],1);
        exercise(0);
        for(unsigned j=0;j<64;++j){
            uint32_t b=0;for(unsigned k=0;k<4;++k)b|=(uint32_t)basis_oracle[(f*64u+j)*4u+k]<<(k*8u);
            uint32_t wanted=(uint32_t)((int32_t)(b*values[v])>>17u);
            REQUIRE(*(uint32_t *)(reference+args[1]-CONTEXT+j*4u)==wanted,"single-frequency transform oracle");
        }
    }
    /* Dense blocks and shifted source/output/basis overlaps remain live. */
    for(unsigned mode=0;mode<32;++mode)for(unsigned a=0;a<4;++a){
        setup(a);args[1]=args[0]+mode*4u;args[2]=mode&1u?args[0]:CONTEXT+0x8000u;exercise(0);
    }
    for(unsigned a=0;a<4;++a){
        setup(a);args[1]=ARGUMENTS+a*4u;args[2]=CONTEXT+0x8000u;exercise(1);
    }
}
static void magnitude(void)
{
    kind=5;stage=10;
    static const uint32_t counts[]={0,1,2,3,4,7,8,15,16,17,24,31,32,33,63,64,65,0x7fffffffu,0x80000000u,UINT32_MAX};
    static const uint32_t reservoir[]={0,0x80000000u,0x7fffffffu,0xffff0000u,UINT32_MAX};
    for(unsigned n=0;n<20;++n)for(unsigned b=0;b<5;++b)for(unsigned a=0;a<4;++a){
        setup(a);args[1]=counts[n];put(args[0]+0x5becu,32,4);put(args[0]+0x5be8u,reservoir[b],4);exercise(0);
    }
    for(unsigned byte=0;byte<256;++byte){
        setup(byte&3u);args[1]=8;put(args[0]+0x5becu,0,4);put(args[0]+0x5be8u,0,4);
        for(unsigned i=0;i<16;++i)put(SOURCE+0x101u+i,byte,1);
        exercise(0);
        REQUIRE(*(uint32_t *)(reference+BYTES)==(byte<128u?byte-255u:byte),"eight-bit sign extension oracle");
    }
    for(unsigned n=0;n<20;++n){
        setup(0);args[0]=ARGUMENTS+8u-0x5be0u;args[1]=counts[n];
        args[2]=SOURCE+0x101u;args[3]=SOURCE+0x109u;args[4]=random32();args[5]=32;exercise(1);
    }
}
static void block_setup(unsigned alignment,unsigned dc,unsigned ac)
{
    setup(alignment);args[1]=0;args[2]=CONTEXT+0x11001u+alignment;args[3]=SOURCE+0x501u;
    put(args[0]+0x98u,0,4);put(args[0]+0x74u,0,4);put(args[0]+0x68u,0,4);
    put(args[0]+0x40b8u,7,4);
    for(unsigned i=0;i<128;++i){put(args[0]+0x40c4u+i*4u,i?i+1u:3u,4);put(args[3]+i,i,1);}
    for(unsigned t=0;t<2;++t){
        uint32_t table=args[0]+0x43c8u+t*0xc0cu;
        put(table,0,4);put(table+4u,0,2);put(table+6u,1,2);
        put(table+0x204u,1,1);put(table+0x205u,1,1);put(table+0x206u,0,1);
        put(table+0x304u,t?ac:dc,1);put(table+0x305u,0,1);
    }
    put(args[0]+0x5becu,32,4);put(args[0]+0x5be8u,0,4);
    for(unsigned i=0;i<256;++i)put(SOURCE+0x101u+i,0,1);
}
static void blocks(void)
{
    stage=11;kind=6;
    for(unsigned run=0;run<16;++run)for(unsigned size=1;size<=15;++size){
        block_setup(run&3u,0,run*16u+size);
        put(args[0]+0x5be8u,1u<<(29u-size),4);exercise(0);
        REQUIRE(*(uint32_t *)(reference+args[2]-CONTEXT)==21u,"DC predictor/dequantization oracle");
        for(unsigned i=1;i<64;++i){
            uint32_t wanted=i==run+1u?(1u-(1u<<size))*(i+1u):0u;
            REQUIRE(*(uint32_t *)(reference+args[2]-CONTEXT+i*4u)==wanted,"one AC coefficient oracle");
        }
    }
    /* All DC categories and AC run/size bytes, including overlong final runs. */
    for(unsigned s=0;s<256;++s){block_setup(s&3u,s,0);exercise(0);block_setup(s&3u,0,s);exercise(0);}
    for(unsigned a=0;a<4;++a)for(unsigned mode=0;mode<6;++mode){
        block_setup(a,0,1);
        if(mode==0)args[2]=args[0]+0x40b8u;
        if(mode==1)args[2]=args[0]+0x40c4u;
        if(mode==2)args[2]=args[0]+0x5be0u;
        if(mode==3)args[2]=args[0]+0x43c8u;
        if(mode==4)args[2]=args[3];
        if(mode==5)args[2]=ARGUMENTS;
        exercise(1);
    }
}
static void expansion(void)
{
    stage=12;kind=7;
    static const uint32_t sizes[]={0,1,2,7,8,9,16,31,0x80000000u,UINT32_MAX};
    for(unsigned w=0;w<10;++w)for(unsigned h=0;h<10;++h)for(unsigned a=0;a<4;++a){
        setup(a);args[1]=CONTEXT+0x8001u+a;args[2]=40;args[3]=2;args[4]=1;args[5]=2u+sizes[w];args[6]=1u+sizes[h];
        exercise(0);
        if(w<8u&&h<8u)for(unsigned y=0;y<sizes[h];++y)for(unsigned x=0;x<sizes[w];++x){
            uint32_t s=args[0]-CONTEXT+(y*8u/sizes[h]*8u+x*8u/sizes[w])*4u;
            uint32_t d=args[1]-CONTEXT+((y+1u)*40u+x+2u)*4u;
            for(unsigned b=0;b<4;++b)REQUIRE(reference[d+b]==initial[s+b],"nearest-neighbour expansion oracle");
        }
    }
    for(unsigned a=0;a<4;++a)for(unsigned shift=0;shift<16;++shift){
        setup(a);args[1]=args[0]+shift;args[2]=8;args[3]=args[4]=0;args[5]=args[6]=8;exercise(0);
    }
    for(unsigned a=0;a<4;++a){setup(a);args[1]=ARGUMENTS+a*4u;args[2]=8;args[3]=args[4]=0;args[5]=args[6]=8;exercise(1);}
}
static void colors(void)
{
    stage=13;kind=8;
    static const uint32_t modes[]={0,1,2,3,4,0x8004u,UINT32_MAX};
    static const int32_t lumas[]={-129,-128,-127,-1,0,1,126,127,128,0x7fffffff,(int32_t)0x80000000u};
    for(unsigned m=0;m<7;++m)for(unsigned v=0;v<11;++v)for(unsigned a=0;a<4;++a){
        setup(a);args[1]=args[2]=0;args[3]=CONTEXT+0x11001u+a;args[4]=modes[m];
        put(args[0],5,4);put(args[0]+4u,3,4);put(args[0]+8u,8,4);put(args[0]+12u,8,4);
        put(args[0]+16u,1,4);put(args[0]+20u,1,4);put(args[0]+0x5bf0u,7,4);
        for(unsigned i=0;i<64;++i){put(args[0]+0xa8u+i*4u,(uint32_t)lumas[v],4);put(args[0]+0x10a8u+i*4u,0,4);put(args[0]+0x20a8u+i*4u,0,4);}
        exercise(0);
        if(v<9u&&(modes[m]==2u||modes[m]==4u)){
            int32_t c=lumas[v]+128;if(c<0)c=0;if(c>255)c=255;
            uint32_t rgb565=((uint32_t)c>>3u)<<11u|((uint32_t)c>>2u)<<5u|((uint32_t)c>>3u);
            for(unsigned y=0;y<3;++y)for(unsigned x=0;x<5;++x){
                uint32_t d=args[3]-CONTEXT+(y*7u+x)*modes[m];
                REQUIRE(reference[d]==(modes[m]==4u?(uint8_t)c:(uint8_t)rgb565),"grayscale output oracle low");
                REQUIRE(reference[d+1u]==(modes[m]==4u?(uint8_t)c:(uint8_t)(rgb565>>8u)),"grayscale output oracle high");
            }
        }
    }
    for(unsigned a=0;a<4;++a)for(unsigned mode=0;mode<5;++mode){
        setup(a);args[1]=1;args[2]=0;args[3]=args[0]+0xa8u;args[4]=mode;
        put(args[0],17,4);put(args[0]+4u,11,4);put(args[0]+8u,8,4);put(args[0]+12u,8,4);
        put(args[0]+16u,1,4);put(args[0]+20u,1,4);put(args[0]+0x5bf0u,22,4);exercise(1);
    }
}
static void traversal(void)
{
    stage=14;
    for(unsigned a=0;a<4;++a)for(unsigned sampling=1;sampling<=2;++sampling)for(unsigned restart=0;restart<3;++restart){
        block_setup(a,0,0);uint32_t c=args[0];
        put(c,17,4);put(c+4u,9,4);put(c+16u,sampling,4);put(c+20u,sampling,4);
        put(c+0x18u,3,4);put(c+0x58u,3,4);put(c+0xa4u,restart,4);put(c+0x5bf0u,19,4);
        for(unsigned i=0;i<3;++i){
            put(c+0x1cu+i*4u,i+1u,4);put(c+0x5cu+i*4u,i+1u,4);
            put(c+0x34u+i*4u,i?1u:sampling,4);put(c+0x40u+i*4u,i?1u:sampling,4);
            put(c+0x4cu+i*4u,0,4);put(c+0x68u+i*4u,0,4);put(c+0x74u+i*4u,0,4);
            put(c+0x98u+i*4u,0,4);put(c+0x40b8u+i*4u,0,4);
            put(c+0x80u+i*4u,i?1u:sampling,4);put(c+0x8cu+i*4u,i?1u:sampling,4);
        }
        for(unsigned i=0;i<sizeof(basis_oracle);++i)put(c+0x5bf4u+i,basis_oracle[i],1);
        kind=9;args[1]=SOURCE+0x501u;put(c+8u,sampling*8u,4);put(c+12u,sampling*8u,4);exercise(0);
        kind=10;args[1]=CONTEXT+0x11000u;args[2]=4;exercise(0);
    }
    stage=15;
    for(unsigned a=0;a<4;++a)for(unsigned width=0;width<4;++width)for(unsigned truncated=0;truncated<3;++truncated){
        setup(a);uint32_t p=SOURCE+0x101u;
        uint8_t header[]={255,216,255,192,0,11,8,0,3,0,(uint8_t)width,1,1,17,0,255,218,0,8,1,1,0,0,63,0};
        for(unsigned i=0;i<sizeof(header);++i)put(p+i,header[i],1);
        args[1]=CONTEXT+0x11000u;args[2]=truncated==0?sizeof(header):truncated==1?17u:1u;args[3]=p;kind=11;exercise(0);
        REQUIRE(*(uint32_t *)(reference+BYTES)==(!truncated&&width?1u:0u),"public JPEG info result oracle");
    }
}
static void composed(void)
{
    stage=7;REQUIRE(map(SOURCE,0x80000u,0)&&map(OUTPUT,OUTPUT_BYTES,0),"composition mappings");
    for(unsigned file=0;file<sizeof(corpus)/sizeof(corpus[0]);++file)for(unsigned variant=0;variant<3;++variant)
    for(implementation=0;implementation<3;++implementation){
        scenario=file*3u+variant;
        load(implementation);int fd=syscall3(5,(uint32_t)corpus[file].path,0,0);REQUIRE(fd>=0,"JPEG open");
        REQUIRE((uint32_t)syscall3(3,fd,SOURCE,0x80000u)==corpus[file].bytes,"JPEG read");syscall3(6,fd,0,0);
        for(unsigned i=0;i<CONTEXT_BYTES;++i)*(volatile uint8_t *)(CONTEXT+i)=(uint8_t)(i*19u+71u);
        for(unsigned i=0;i<OUTPUT_BYTES;++i)*(volatile uint8_t *)(OUTPUT+i)=0x6d;
        uint32_t call[]={CONTEXT,CONTEXT+0xf000u,corpus[file].bytes,SOURCE,0,0,0,0},actual[9];
        REQUIRE(!raster_checked_call(implementation==1u?(void *)kernel_jpeg_info_83a70f:(void *)0x83a70f,call,actual)&&actual[0]==1u,"real JPEG info");
        REQUIRE(D(CONTEXT+0xf008u)==corpus[file].width&&D(CONTEXT+0xf00cu)==corpus[file].height,"real dimensions");
        call[1]=corpus[file].bytes;call[2]=SOURCE;call[3]=variant==1u?2u:variant==2u?0x8004u:4u;call[4]=OUTPUT;call[5]=variant==2u?13u:0u;
        REQUIRE(!raster_checked_call(implementation==1u?(void *)kernel_jpeg_decode_83a7a1:(void *)0x83a7a1,call,actual)&&!actual[0],"real JPEG decode");
        unsigned changed=0;
        for(unsigned i=0;i<OUTPUT_BYTES;++i){uint8_t v=*(volatile uint8_t *)(OUTPUT+i);if(v!=0x6d)++changed;if(!implementation)reference_pixels[i]=v;else REQUIRE(v==reference_pixels[i],"real pixel/slack comparison");}
        REQUIRE(changed>corpus[file].width*corpus[file].height,"real image output");
        for(unsigned i=0;i<CONTEXT_BYTES;++i){uint8_t v=*(volatile uint8_t *)(CONTEXT+i);if(!implementation)reference_context[i]=v;else REQUIRE(v==reference_context[i],"real workspace comparison");}
        ++real_decodes;
    }
}
void _start(void)
{
    REQUIRE(map(CONTEXT,CONTEXT_BYTES,0)&&map(SOURCE,SOURCE_BYTES,0)&&map(0x640000,4096,0)&&map(0x800000,0x40000,0)&&map(SHARED,0x23000,1),"mappings");
    markers();preparation();symbols();probes();basis();transform();magnitude();blocks();expansion();colors();traversal();composed();
    REQUIRE(faults&&safe_probes&&per_kind[0]&&per_kind[1]&&per_kind[2],"coverage");
    PRINT("JPEG scan scenarios/executions: ");number(cases);PRINT(" ");number(cases*3u);
    PRINT("; dispatcher/prepare/symbol/basis/transform/magnitude/block/expand/color/MCU/image/info/decode scenarios: ");for(unsigned i=0;i<13;++i){number(per_kind[i]);PRINT(" ");}
    PRINT("; fault/loop/safe probe executions/caller changes: ");number(faults);PRINT(" ");number(loops);PRINT(" ");number(safe_probes);PRINT(" ");number(caller_changes);
    PRINT("; real JPEG info/decode executions: ");number(real_decodes);PRINT("; jpeg scan verified with full state/source/caller/EAX/ABI and real pixel/workspace comparisons; no helper substitutions\n");
    syscall3(1,0,0,0);
}
