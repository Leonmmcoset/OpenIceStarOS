#include "../kernel/syscall_services.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define PRINT(s) syscall3(4,1,(uint32_t)(s),sizeof(s)-1)
static const uint32_t values[]={0,1,0x7f,0x80,0xff,0x100,0x7fffffff,0x80000000,0xffffffff};
static const uint32_t indices[]={0,1,2,3,255,256,511,1023};
static const uint32_t addresses[]={0x81041a,0x810448,0x810478,0x82a9bb};
static const uint32_t candidates[]={(uint32_t)kernel_syscall_target_81041a,(uint32_t)kernel_syscall_target_810448,
    (uint32_t)kernel_syscall_target_810478,(uint32_t)kernel_syscall_target_82a9bb};
static uint8_t saved[0x6000];
static uint32_t seed,hits,mutation,cases,faults;
static int syscall3(int n,uint32_t a,uint32_t b,uint32_t c)
{int r;__asm__ volatile("int $0x80":"=a"(r):"a"(n),"b"(a),"c"(b),"d"(c):"memory");return r;}
static int map(uint32_t address,uint32_t size)
{uint32_t args[]={address,size,7,0x32,0xffffffff,0};return (uint32_t)syscall3(90,(uint32_t)args,0,0)==address;}
static int load(const char *path,uint32_t address)
{
    int fd=syscall3(5,(uint32_t)path,0,0),n;if(fd<0)return 0;
    n=syscall3(3,(uint32_t)fd,address,0x40000);syscall3(6,(uint32_t)fd,0,0);return n==250099;
}
static void jump(uint32_t address,uint32_t target)
{*(uint8_t *)address=0xe9;WORD(address+1)=target-address-5;}
uint32_t kernel_timestamp_low(void)
{++hits;WORD(0x600ffc)^=mutation;return seed;}
static uint32_t candidate(uint32_t function)
{
#ifdef KERNEL_LINKED_TEST
    return addresses[function];
#else
    return candidates[function];
#endif
}
static uint32_t invoke(uint32_t entry,uint32_t a,uint32_t b)
{return ((uint32_t (*)(uint32_t,uint32_t))(uintptr_t)entry)(a,b);}
static uint32_t oracle(uint32_t f,uint32_t a,uint32_t b,uint32_t object)
{
    if(!f)return b;
    if(f==1)return WORD(object+12);
    if(f==2)return WORD(object+4);
    uint32_t n=seed;n^=n>>6;n^=n<<12;n^=n>>14;return n%(b-a+1)+a;
}
static void initialize(uint32_t pattern,uint32_t index,uint32_t object)
{
    uint32_t i;
    for(i=0;i<0x4000;++i)((volatile uint8_t *)0x600000)[i]=(uint8_t)(i*37+pattern*83);
    for(i=0;i<0x2000;++i)((volatile uint8_t *)0x6a0000)[i]=(uint8_t)(i*17+pattern*59);
    WORD(0x6a0000+4*index)=object;hits=0;
}
static int exercise(uint32_t f,uint32_t pattern,uint32_t a,uint32_t b,uint32_t object)
{
    uint32_t result,expected,i;
    initialize(pattern,f==3?0:a,object);expected=oracle(f,a,b,object);
    result=invoke(addresses[f]+0x100000,a,b);
    if(result!=expected || hits!=(f==3))return 1;
    if(!f && WORD(object+0x55f)!=b)return 2;
    for(i=0;i<0x4000;++i)saved[i]=((volatile uint8_t *)0x600000)[i];
    for(i=0;i<0x2000;++i)saved[0x4000+i]=((volatile uint8_t *)0x6a0000)[i];
    initialize(pattern,f==3?0:a,object);
    result=invoke(candidate(f),a,b);
    if(result!=expected || hits!=(f==3))return 3;
    for(i=0;i<0x4000;++i)if(saved[i]!=((volatile uint8_t *)0x600000)[i])return 4;
    for(i=0;i<0x2000;++i)if(saved[0x4000+i]!=((volatile uint8_t *)0x6a0000)[i])return 5;
    ++cases;return 0;
}
static int divide_fault(uint32_t lower,int original)
{
    int child=syscall3(2,0,0,0),status=0;
    if(child<0)return 0;
    if(!child){invoke(original?0x92a9bb:candidate(3),lower,lower-1);syscall3(1,91,0,0);}
    return syscall3(7,(uint32_t)child,(uint32_t)&status,0)==child && (status&0x7f)==8;
}
static void number(uint32_t n)
{char s[10];uint32_t i=10;do{s[--i]='0'+n%10;n/=10;}while(n);syscall3(4,1,(uint32_t)(s+i),10-i);}
static int run(void)
{
    uint32_t f,p,index,wrap,align,v,s,u,error;
    if(!map(0x600000,0x4000)||!map(0x6a0000,0x2000)||!map(0x900000,0x40000)||!load("build/icestar.x",0x900000))return 1;
    jump(0x93734a,(uint32_t)kernel_timestamp_low);
#ifdef KERNEL_LINKED_TEST
    (void)candidates;
    if(!map(0x800000,0x40000)||!load("build/openicestaros_kernel.x",0x800000))return 2;
    jump(0x83734a,(uint32_t)kernel_timestamp_low);
#endif
    for(f=0;f<3;++f)for(p=0;p<3;++p)for(index=0;index<COUNT(indices);++index)
    for(wrap=0;wrap<4;++wrap)for(align=0;align<4;++align)for(v=0;v<COUNT(values);++v){
        error=exercise(f,p,indices[index]+wrap*0x40000000u,values[v],0x600000+align);
        if(error)return 10+error;
    }
    for(p=0;p<3;++p)for(s=0;s<COUNT(values);++s)for(v=0;v<COUNT(values);++v)for(u=0;u<COUNT(values);++u){
        seed=values[s];mutation=0x23456789*p;
        if(values[u]-values[v]+1==0){
            if(!divide_fault(values[v],0)||!divide_fault(values[v],1))return 20;
            faults+=2;continue;
        }
        error=exercise(3,p,values[v],values[u],0x600000);if(error)return 30+error;
    }
    PRINT("Syscall object access/random original/C/oracle cases: ");number(cases);
    PRINT("; wrapped indices, unaligned fields, raw returns, seed calls and memory verified\n");
    PRINT("Original/C zero-span SIGFPE cases: ");number(faults);PRINT("\n");return 0;
}
void _start(void){syscall3(1,run(),0,0);__builtin_unreachable();}
