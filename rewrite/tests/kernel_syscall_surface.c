/* Execute the original and C surface forwarder against an independent
 * callback oracle. The compositor body is an observation/mutation boundary;
 * this fixture does not claim to recover that still-original implementation. */
#include "../kernel/syscall_surface.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define PRINT(s) syscall3(4,1,(uint32_t)(s),sizeof(s)-1)
static const uint32_t values[]={0,1,0x7f,0x80,0xff,0x100,0x7fffffff,0x80000000,0xffffffff};
static const uint32_t indices[]={0,1,511,2047};
static const uint32_t starts[]={0x1b000,0x500000,0x600000,0x610000,0x6a0000};
static const uint32_t lengths[]={0x1000,0x2000,0x1000,0x1000,0x2000};
static uint8_t expected[0x7000];
static uint32_t trace[5],expected_args[5],calls,mode,pattern,object,surface,index,layer,value;
static uint32_t return_word,cases,faults;
uint32_t surface_input_after,surface_canary_after;
static int syscall3(int n,uint32_t a,uint32_t b,uint32_t c)
{int r;__asm__ volatile("int $0x80":"=a"(r):"a"(n),"b"(a),"c"(b),"d"(c):"memory");return r;}
static int map(uint32_t address,uint32_t size)
{uint32_t args[]={address,size,7,0x32,0xffffffff,0};return (uint32_t)syscall3(90,(uint32_t)args,0,0)==address;}
static int load(const char *path,uint32_t address)
{int fd=syscall3(5,(uint32_t)path,0,0),n;if(fd<0)return 0;n=syscall3(3,(uint32_t)fd,address,0x40000);syscall3(6,(uint32_t)fd,0,0);return n==250099;}
static void jump(uint32_t address,uint32_t target)
{*(uint8_t *)address=0xe9;WORD(address+1)=target-address-5;}
static uint32_t candidate(void)
{
#ifdef KERNEL_LINKED_TEST
    return 0x81017c;
#else
    return (uint32_t)kernel_syscall_target_81017c;
#endif
}
__attribute__((naked,noinline)) static uint32_t invoke(
    uint32_t entry __attribute__((unused)),uint32_t argument __attribute__((unused)))
{
    __asm__("pushl %ebp;movl %esp,%ebp;subl $24,%esp;"
        "movl 12(%ebp),%eax;movl %eax,0(%esp);movl $0x93fedc71,4(%esp);"
        "call *8(%ebp);movl 0(%esp),%edx;movl %edx,surface_input_after;"
        "movl 4(%esp),%edx;movl %edx,surface_canary_after;leave;ret");
}
static void effects(void)
{
    if(mode&1){WORD(object+12)^=0xc0000001;WORD(0x6a0000+4*index)=0x12345678;WORD(0x500000+4*layer)=0xfedcba98;}
    if(mode&2){WORD(surface+4)^=0x91a2b3c4;WORD(surface+8)^=0xfedcba98;WORD(surface+12)^=0x43218765;WORD(surface+16)^=0x798a9bac;}
    WORD(0x1b080)^=return_word;
}
__attribute__((used,noinline)) static uint32_t observe(volatile uint32_t *args)
{
    uint32_t i;++calls;
    for(i=0;i<5;++i){trace[i]=args[i];args[i]=0xa5a50000|i;}
    effects();return return_word;
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
__attribute__((naked,noinline)) uint32_t kernel_syscall_target_81905e(
    uint32_t a0,uint32_t a1,uint32_t a2,uint32_t a3,uint32_t a4)
{
    __asm__("pushl %ebp;movl %esp,%ebp;leal 8(%ebp),%eax;pushl %eax;"
        "call observe;addl $4,%esp;leave;ret");
}
#pragma GCC diagnostic pop
static void initialize(void)
{
    uint32_t region,i;
    for(region=0;region<COUNT(starts);++region)for(i=0;i<lengths[region];++i)
        ((volatile uint8_t *)starts[region])[i]=(uint8_t)(i*37+pattern*83+region*59);
    WORD(0x6a0000+4*index)=object;WORD(object+12)=layer;WORD(0x500000+4*layer)=surface;
    for(i=0;i<4;++i)WORD(surface+4+4*i)=values[(value+i)%COUNT(values)];
    calls=0;for(i=0;i<5;++i)trace[i]=0;
}
static void save(void)
{
    uint32_t region,i,offset=0;
    for(region=0;region<COUNT(starts);++region)for(i=0;i<lengths[region];++i)
        expected[offset++]=((volatile uint8_t *)starts[region])[i];
}
static int compare(void)
{
    uint32_t region,i,offset=0;
    for(region=0;region<COUNT(starts);++region)for(i=0;i<lengths[region];++i)
        if(expected[offset++]!=((volatile uint8_t *)starts[region])[i])return 0;
    return 1;
}
static int exercise(void)
{
    uint32_t phase,i,result;
    expected_args[0]=layer;
    for(i=0;i<4;++i)expected_args[i+1]=values[(value+i)%COUNT(values)];
    initialize();effects();save();
    for(phase=0;phase<2;++phase){
        initialize();result=invoke(phase?candidate():0x91017c,index);
        if(result!=return_word || calls!=1 || surface_input_after!=index || surface_canary_after!=0x93fedc71)return 1;
        for(i=0;i<5;++i)if(trace[i]!=expected_args[i])return 2;
        if(!compare())return 3;
    }
    ++cases;return 0;
}
static int fault(uint32_t target)
{
    int child=syscall3(2,0,0,0),status=0;
    if(child<0)return 0;
    if(!child){invoke(target,index);syscall3(1,91,0,0);}
    return syscall3(7,(uint32_t)child,(uint32_t)&status,0)==child && (status&0x7f)==11;
}
static void number(uint32_t n)
{char s[10];uint32_t i=10;do{s[--i]='0'+n%10;n/=10;}while(n);syscall3(4,1,(uint32_t)(s+i),10-i);}
static int run(void)
{
    uint32_t region,n,wrap,lwrap,align,error,offset,kind;
    for(region=0;region<COUNT(starts);++region)if(!map(starts[region],lengths[region]))return 1;
    if(!map(0x900000,0x40000)||!load("build/icestar.x",0x900000))return 2;
    jump(0x91905e,(uint32_t)kernel_syscall_target_81905e);
#ifdef KERNEL_LINKED_TEST
    if(!map(0x800000,0x40000)||!load("build/openicestaros_kernel.x",0x800000))return 3;
    jump(0x81905e,(uint32_t)kernel_syscall_target_81905e);
#endif
    for(pattern=0;pattern<2;++pattern)for(n=0;n<COUNT(indices);++n)for(wrap=0;wrap<4;++wrap)
    for(lwrap=0;lwrap<4;++lwrap)for(align=0;align<4;++align)
    for(value=0;value<COUNT(values);++value)for(mode=0;mode<4;++mode){
        index=indices[n]+wrap*0x40000000u;layer=indices[(n+1)%COUNT(indices)]+lwrap*0x40000000u;
        object=0x600000+align;surface=0x610000+((align+1)&3);return_word=values[(value+7)%COUNT(values)];
        error=exercise();if(error)return 10+error;
    }
    /* Last valid DWORDs must work without reading adjacent unmapped pages. */
    mode=0;index=2047;layer=2047;object=0x600ff0;surface=0x610fec;
    if(exercise())return 20;
    /* Force the highest needed field to straddle a guard page by 1/2/3
     * bytes. Both original and C must retain a full-width faulting read. */
    for(kind=0;kind<3;++kind)for(offset=1;offset<4;++offset){
        object=0x600ff0;surface=0x610fec;index=2047;layer=2047;initialize();
        if(kind==0)WORD(0x6a0000+4*index)=object+offset;
        if(kind==1)WORD(0x500000+4*layer)=surface+offset;
        if(kind==2)index=2048;
        if(!fault(0x91017c)||!fault(candidate()))return 21;
        faults+=2;
    }
    PRINT("Syscall surface forwarder original/C/oracle cases: ");number(cases);
    PRINT("; wrapped indices, unaligned fields, raw returns, callback writes and memory verified\n");
    PRINT("Original/C surface guard-page SIGSEGV cases: ");number(faults);PRINT("\n");return 0;
}
void _start(void){syscall3(1,run(),0,0);__builtin_unreachable();}
