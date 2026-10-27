/* Original x86 / recovered C / byte-addressed independent model. The model
 * validates all reads, writes and loop bounds before executable invocation. */
#include "../kernel/surface_scale.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define PRINT(s) syscall3(4,1,(uint32_t)(s),sizeof(s)-1)
#define ARGS 0x630800u
static const uint32_t regions[][2] = {
    {0x500000,0x1000},{0x600000,0x1000},{0x610000,0x1000},
    {0x620000,0x1000},{ARGS,0x800}};
static const uint32_t dimensions[][2] = {
    {1,1},{2,2},{3,3},{5,5},{9,9},{1,2},{2,3},{3,5},{5,9},
    {2,9},{9,2},{9,5},{5,3},{3,2},{2,1},
    {1,0},{0,0},{0,1},{0xffffffff,1},{1,0xffffffff},
    {0xffffffff,0xffffffff},{0x80000000,0x80000000},
    {0x7fffffff,0x80000000},{0x80000000,0x7fffffff},
    {0x1fffffff,0xffffffff},{0,0x80000000},{1,0x80000000},
    {0x80000000,1},{1,0x7fffffff},{0x20000000,1}};
static const uint32_t depths[][2] = {{8,8},{32,32},{8,32},{32,8},{16,16},{0xffffffff,8}};
static const uint32_t origins[][2] = {
    {0,0},{2,3},{0xffffffff,0xfffffffe},
    {0x80000000,0x80000000},{0x7fffffff,0x7fffffff}};
static uint8_t expected[0x4800];
static uint32_t align,alias,depth,wi,hi,origin,metadata,variant;
static uint32_t dest_object,source_object,dest_layer,source_layer;
static uint32_t cases,skipped,faults,model_faults,coverage,unsafe,steps;
static uint32_t mismatch,meta_cases,argument_cases;
__attribute__((used)) static uint32_t saved_stack;
static int syscall3(int n,uint32_t a,uint32_t b,uint32_t c)
{int r;__asm__ volatile("int $0x80":"=a"(r):"a"(n),"b"(a),"c"(b),"d"(c):"memory");return r;}
static int map(uint32_t address,uint32_t size)
{uint32_t a[]={address,size,7,0x32,0xffffffff,0};return (uint32_t)syscall3(90,(uint32_t)a,0,0)==address;}
static int load(const char *path,uint32_t address)
{int fd=syscall3(5,(uint32_t)path,0,0),n;if(fd<0)return 0;n=syscall3(3,(uint32_t)fd,address,0x40000);syscall3(6,(uint32_t)fd,0,0);return n==250099;}
static void number(uint32_t n)
{char s[10];uint32_t i=10;do{s[--i]='0'+n%10;n/=10;}while(n);syscall3(4,1,(uint32_t)(s+i),10-i);}
static int64_t signed_word(uint32_t n)
{return n<0x80000000u?(int64_t)n:(int64_t)n-INT64_C(4294967296);}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
__attribute__((naked,noinline)) static uint32_t invoke(uint32_t target)
{
    __asm__("movl %esp,saved_stack;movl 4(%esp),%eax;"
        "movl $0x630800,%esp;call *%eax;movl saved_stack,%esp;ret");
}
#pragma GCC diagnostic pop
static uint32_t candidate(void)
{
#ifdef KERNEL_LINKED_TEST
    return 0x8194e4;
#else
    return (uint32_t)kernel_surface_scale;
#endif
}
static void initialize(void)
{
    uint32_t r,i,size=depths[depth][0]==32?4:1,output,input,control=0,target=0;
    for(r=0;r<COUNT(regions);++r)for(i=0;i<regions[r][1];++i)
        BYTE(regions[r][0]+i)=(uint8_t)(i*37+r*59+wi*7+hi*13);
    dest_object=0x600100+align;source_object=0x600200+align;
    dest_layer=1+align*0x40000000u;source_layer=2+(3-align)*0x40000000u;
    for(i=0;i<16;++i)WORD(0x500000+4*i)=source_object;
    WORD(0x500004)=dest_object;
    output=0x610400+align;input=0x620400+align;
    if(alias)input=output+(alias==1?0:alias==2?1:alias==3?0xffffffff:alias==4?size:0u-size);
    WORD(ARGS)=dest_layer;WORD(ARGS+4)=source_layer;
    WORD(ARGS+8)=origins[origin][0];WORD(ARGS+12)=origins[origin][1];
    WORD(ARGS+16)=dimensions[wi][1];WORD(ARGS+20)=dimensions[hi][1];
    WORD(ARGS+24)=0x93fedc71;
    for(i=0;i<3;++i){
        uint32_t object=dest_object+i*0x100;
        WORD(object)=i?input:output;
        WORD(object+12)=i?dimensions[wi][0]:16;
        WORD(object+16)=i?dimensions[hi][0]:16;
        WORD(object+36)=depths[depth][i?1:0];
    }
    if(!metadata)return;
    /* Controlled stores can change descriptors, layer-table entries or the
     * actual caller argument words. A second descriptor remains mapped. */
    if(metadata<=4)target=dest_object+(metadata==1?0:metadata==2?12:metadata==3?16:36);
    else if(metadata<=8)target=source_object+(metadata==5?0:metadata==6?12:metadata==7?16:36);
    else if(metadata<=10)target=0x500000+4*(metadata==9?1:2);
    else target=ARGS+4*(metadata-11);
    if(metadata==1||metadata==5)control=0x620400+align;
    else if(metadata==9||metadata==10)control=source_object+0x100;
    else if(metadata==11||metadata==12)control=2;
    else if(metadata==13||metadata==14)control=0;
    else if(metadata==4||metadata==8)control=16;
    else control=variant==0?1:variant==1?4:variant==2?0:variant==3?0xffffffff:variant==4?0x80000000:0x7fffffff;
    WORD(ARGS+8)=WORD(ARGS+12)=0;
    WORD(dest_object)=target;
    /* Sparse 8-bit writes only replace the low byte; that is intentional. */
    for(i=0;i<128;++i)if(size==4)WORD(input+4*i)=control;else BYTE(input+i)=(uint8_t)control;
}
static void capture(void)
{uint32_t r,i,n=0;for(r=0;r<COUNT(regions);++r)for(i=0;i<regions[r][1];++i)expected[n++]=BYTE(regions[r][0]+i);}
static int locate(uint32_t address,uint32_t length)
{
    uint32_t r,offset=0;
    for(r=0;r<COUNT(regions);++r){
        if(length<=regions[r][1] && address>=regions[r][0] &&
           address-regions[r][0]<=regions[r][1]-length)
            return (int)(offset+address-regions[r][0]);
        offset+=regions[r][1];
    }
    unsafe=1;return -1;
}
static uint32_t read_bytes(uint32_t address,uint32_t size)
{
    int offset=locate(address,size);uint32_t i,v=0;
    if(offset<0)return 0;
    for(i=0;i<size;++i)v|=(uint32_t)expected[(uint32_t)offset+i]<<(8*i);
    return v;
}
static void write_bytes(uint32_t address,uint32_t size,uint32_t value)
{
    int offset=locate(address,size);uint32_t i;
    if(offset<0)return;
    for(i=0;i<size;++i)expected[(uint32_t)offset+i]=(uint8_t)(value>>(8*i));
}
static uint32_t arg(uint32_t i){return read_bytes(ARGS+4*i,4);}
static uint32_t descriptor(uint32_t i){return read_bytes(0x500000+4*arg(i),4);}
static uint32_t field(uint32_t i,uint32_t offset){return read_bytes(descriptor(i)+offset,4);}
static uint32_t quotient(uint32_t numerator,uint32_t denominator)
{
    if(!denominator || (numerator==0x80000000 && denominator==0xffffffff)){
        unsafe=2;return 0;
    }
    return (uint32_t)((int32_t)numerator/(int32_t)denominator);
}
static uint32_t ratio(uint32_t offset,uint32_t input)
{
    uint32_t s=field(1,offset),d=arg(input);
    if(signed_word(s)>signed_word(d))return quotient((s-d)*100,d);
    if(signed_word(s)<signed_word(d))return quotient(s*100,d-s);
    return 0;
}
static int tick(void){if(++steps>1024)unsafe=1;return !unsafe;}
static void copy_pixel(uint32_t out,uint32_t in,uint32_t size)
{
    uint32_t pixel=read_bytes(in,size);
    if(size==4)pixel&=0xffffff;
    write_bytes(out,size,pixel);
}
static uint32_t oracle(void)
{
    uint32_t rx,ry,obj,size,row=0,sy=0,vc=1;
    unsafe=steps=0;rx=ratio(12,4);if(unsafe)return 0;
    ry=ratio(16,5);if(unsafe)return 0;
    if(field(0,36)==32 && field(1,36)==32){size=4;coverage|=1;}
    else {
        obj=descriptor(0);
        if(read_bytes(obj+36,4)!=8){coverage|=4;return obj;}
        obj=descriptor(1);
        if(read_bytes(obj+36,4)!=8){coverage|=8;return obj;}
        size=1;coverage|=2;
    }
    for(;signed_word(row)<signed_word(arg(5)) && tick();++row,++sy){
        uint32_t dest,source,hc=1,col=0,sx=0,offset;
        if(signed_word(field(1,16))<=signed_word(sy)){
            sy=field(1,16)-1;coverage|=16;
        }
        obj=descriptor(0);offset=(arg(3)+row)*field(0,12)+arg(2);
        dest=read_bytes(obj,4)+size*offset;
        obj=descriptor(1);offset=field(1,12)*sy;
        source=read_bytes(obj,4)+size*offset;
        for(;signed_word(col)<signed_word(arg(4)) && tick();++col,++sx){
            if(signed_word(field(1,12))<=signed_word(sx)){
                sx=field(1,12)-1;coverage|=32;
            }
            copy_pixel(dest+size*col,source+size*sx,size);if(unsafe)return 0;
            if(signed_word(field(1,12))>signed_word(arg(4))){
                coverage|=64;
                for(;;){
                    if(signed_word(hc*100)>=signed_word(col*rx))break;
                    if(signed_word(field(1,12)-1)<=signed_word(sx))break;
                    if(!tick())return 0;
                    ++sx;++hc;coverage|=128;
                }
            }else if(signed_word(field(1,12))<signed_word(arg(4))){
                coverage|=256;
                for(;;){
                    if(signed_word((col+1)*100)<signed_word(hc*rx))break;
                    if(signed_word(col+1)>=signed_word(arg(4)))break;
                    if(!tick())return 0;
                    copy_pixel(dest+size*(col+1),source+size*sx,size);
                    if(unsafe)return 0;
                    ++col;++hc;coverage|=512;
                }
            }
        }
        if(unsafe)return 0;
        if(signed_word(field(1,16))>signed_word(arg(5))){
            coverage|=1024;
            for(;;){
                if(signed_word(vc*100)>=signed_word(row*ry))break;
                if(signed_word(field(1,16)-1)<=signed_word(sy))break;
                if(!tick())return 0;
                ++sy;++vc;coverage|=2048;
            }
        }else if(signed_word(field(1,16))<signed_word(arg(5))){
            coverage|=4096;
            if(signed_word((row+1)*100)>=signed_word(vc*ry) &&
                signed_word(row+1)<signed_word(arg(5))){--sy;++vc;coverage|=8192;}
        }
    }
    return row;
}
static int compare(void)
{
    uint32_t r,i,n=0;
    for(r=0;r<COUNT(regions);++r)for(i=0;i<regions[r][1];++i)
        if(expected[n++]!=BYTE(regions[r][0]+i)){
            mismatch=regions[r][0]+i;return 0;
        }
    return 1;
}
static int exercise(void)
{
    uint32_t expected_result,result,phase;
    initialize();capture();expected_result=oracle();
    if(unsafe){if(unsafe==2)++model_faults;else ++skipped;return 0;}
    for(phase=0;phase<2;++phase){
        initialize();result=invoke(phase?candidate():0x9194e4);
        if(result!=expected_result || !compare()){
            PRINT("Surface scale mismatch phase/case/width/height/depth/alignment/alias/origin/metadata/variant/address/expected/actual: ");
            number(phase);PRINT("/");number(cases);PRINT("/");number(wi);PRINT("/");number(hi);
            PRINT("/");number(depth);PRINT("/");number(align);PRINT("/");number(alias);
            PRINT("/");number(origin);PRINT("/");number(metadata);PRINT("/");number(variant);
            PRINT("/");number(mismatch);PRINT("/");number(expected_result);PRINT("/");number(result);PRINT("\n");
            return 1;
        }
    }
    ++cases;if(metadata)++meta_cases;if(metadata>=11)++argument_cases;
    return 0;
}
static int divide_fault(uint32_t target)
{
    int child=syscall3(2,0,0,0),status=0;
    if(child<0)return 0;
    if(!child){initialize();invoke(target);syscall3(1,91,0,0);}
    return syscall3(7,(uint32_t)child,(uint32_t)&status,0)==child && (status&127)==8;
}
static int run(void)
{
    uint32_t i,limits[2]={0,0};
    syscall3(75,4,(uint32_t)limits,0);
    for(i=0;i<4;++i)if(!map(regions[i][0],0x1000))return 1;
    if(!map(0x630000,0x1000)||!map(0x900000,0x40000)||!load("build/icestar.x",0x900000))return 2;
#ifdef KERNEL_LINKED_TEST
    if(!map(0x800000,0x40000)||!load("build/icestar_c.x",0x800000))return 3;
#endif
    for(wi=0;wi<15;++wi)for(hi=0;hi<15;++hi)for(depth=0;depth<2;++depth)
    for(align=0;align<4;++align)for(alias=0;alias<6;++alias)for(origin=0;origin<3;++origin)
        if(exercise())return 10;
    /* All signed boundary dimension pairs and all depth rejection branches. */
    alias=0;
    for(wi=0;wi<COUNT(dimensions);++wi)for(hi=0;hi<COUNT(dimensions);++hi)
    for(depth=0;depth<COUNT(depths);++depth)for(align=0;align<4;++align){
        origin=(wi+hi)%COUNT(origins);if(exercise())return 11;
    }
    wi=6;hi=12;origin=alias=0;
    for(metadata=1;metadata<=16;++metadata)for(variant=0;variant<6;++variant)
    for(depth=0;depth<2;++depth)for(align=0;align<4;++align)
        if(exercise())return 12;
    metadata=alias=origin=0;
    /* Horizontal/vertical divide by zero and quotient overflow. Both must
     * happen before depth rejection, including unsupported destinations. */
    for(i=0;i<4;++i)for(depth=0;depth<COUNT(depths);++depth)for(align=0;align<4;++align){
        wi=i<2?(i==0?15:24):0;hi=i>=2?(i==2?15:24):0;
        if(!divide_fault(0x9194e4)||!divide_fault(candidate()))return 13;
        faults+=2;
    }
    if(coverage!=16383||meta_cases<200||argument_cases<100)return 14;
    PRINT("Surface scale original/C/byte-oracle cases: ");number(cases);
    PRINT("; explicit unsafe skips: ");number(skipped);PRINT("; modeled divide faults: ");number(model_faults);
    PRINT("; matching SIGFPE executions: ");number(faults);PRINT("; metadata aliases: ");number(meta_cases);
    PRINT("; caller aliases: ");number(argument_cases);
    PRINT("; 8/32-bit shrink/enlarge/equal paths, live words, forward aliases, signed/wrapped arithmetic, raw returns and full mapped data verified\n");
    return 0;
}
void _start(void){syscall3(1,run(),0,0);__builtin_unreachable();}
