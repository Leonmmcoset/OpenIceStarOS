/* Compare original machine code, recovered C and bytewise independent oracle.
 * Aliases include the signed weight words themselves, not just pixel buffers. */
#include "../kernel/surface_alpha.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define PRINT(s) syscall3(4,1,(uint32_t)(s),sizeof(s)-1)
static const uint32_t bases[]={0x610000,0x620000,0x630000};
static const uint32_t values[]={0,1,2,31,63,64,65,126,127,128,129,255,
    0x7fffffff,0x80000000,0xfffffffe,0xffffffff};
static const int32_t offsets[]={-15,-12,-5,-4,-3,-1,0,1,2,3,4,5,8,12,15};
static uint8_t expected[0x3000];
static uint32_t destination,source,weights,pattern,alignment,alias,which,mode,cases,coverage;
static int syscall3(int n,uint32_t a,uint32_t b,uint32_t c)
{int r;__asm__ volatile("int $0x80":"=a"(r):"a"(n),"b"(a),"c"(b),"d"(c):"memory");return r;}
static int map(uint32_t address,uint32_t size)
{uint32_t a[]={address,size,7,0x32,0xffffffff,0};return (uint32_t)syscall3(90,(uint32_t)a,0,0)==address;}
static int load(const char *path,uint32_t address)
{int fd=syscall3(5,(uint32_t)path,0,0),n;if(fd<0)return 0;n=syscall3(3,(uint32_t)fd,address,0x40000);syscall3(6,(uint32_t)fd,0,0);return n==250099;}
static void number(uint32_t n)
{char s[10];uint32_t i=10;do{s[--i]='0'+n%10;n/=10;}while(n);syscall3(4,1,(uint32_t)(s+i),10-i);}
static uint32_t get(uint32_t address)
{uint32_t n=0,i;for(i=0;i<4;++i)n|=(uint32_t)BYTE(address+i)<<(8*i);return n;}
static void put(uint32_t address,uint32_t n)
{uint32_t i;for(i=0;i<4;++i)BYTE(address+i)=(uint8_t)(n>>(8*i));}
static int64_t signed_word(uint32_t n)
{return n&0x80000000u?(int64_t)n-0x100000000LL:n;}
static void initialize(void)
{
    uint32_t r,i;
    for(r=0;r<COUNT(bases);++r)for(i=0;i<0x1000;++i)
        BYTE(bases[r]+i)=(uint8_t)(i*37+pattern*83+r*59);
    destination=0x610700+alignment;source=0x620700+alignment;weights=0x630700+alignment;
    if(alias<COUNT(offsets))source=destination+(uint32_t)offsets[alias];
    else if(alias<2*COUNT(offsets))weights=destination+(uint32_t)offsets[alias-COUNT(offsets)];
    else if(alias<3*COUNT(offsets))weights=source+(uint32_t)offsets[alias-2*COUNT(offsets)];
    else if(alias<4*COUNT(offsets)){
        source=destination+(uint32_t)offsets[alias-3*COUNT(offsets)];
        weights=destination+(uint32_t)offsets[(alias+5)%COUNT(offsets)];
    }
    for(i=0;i<4;++i)WORD(weights+4*i)=values[(which+(mode?5*i:0))%COUNT(values)];
}
static uint32_t oracle(void)
{
    uint32_t pixel,result=0;
    for(pixel=0;pixel<4;++pixel){
        uint32_t d=destination+4*pixel,s=source+4*pixel,a=weights+4*pixel;
        int32_t c;
        result=a;
        if(signed_word(get(a))>127){coverage|=1;continue;}
        if(signed_word(get(a))<=0){uint32_t copy=get(s);put(d,copy);result=d;coverage|=2;continue;}
        coverage|=4;
        for(c=2;c>=0;--c){
            uint32_t old=BYTE(d+(uint32_t)c),w=get(a),incoming=BYTE(s+(uint32_t)c);
            uint32_t inverse=128-get(a);
            uint32_t bits=(uint32_t)((uint64_t)old*w+(uint64_t)incoming*inverse);
            int64_t total=signed_word(bits),quotient=total/128;
            if(total<0 && total%128)--quotient;
            result=(uint32_t)quotient;
            BYTE(d+(uint32_t)c)=(uint8_t)result;
            if(result>255)coverage|=8;
        }
    }
    return result;
}
static void save(void)
{uint32_t r,i,n=0;for(r=0;r<COUNT(bases);++r)for(i=0;i<0x1000;++i)expected[n++]=BYTE(bases[r]+i);}
static int compare(void)
{uint32_t r,i,n=0;for(r=0;r<COUNT(bases);++r)for(i=0;i<0x1000;++i)if(expected[n++]!=BYTE(bases[r]+i))return 0;return 1;}
typedef uint32_t (*Blend)(uint32_t,uint32_t,uint32_t);
static int exercise(void)
{
    uint32_t phase,result,expected_result;
    initialize();expected_result=oracle();save();
    for(phase=0;phase<2;++phase){
        Blend blend;
        initialize();
#ifdef KERNEL_LINKED_TEST
        blend=(Blend)(phase?0x818134:0x918134);
#else
        blend=phase?kernel_surface_blend4:(Blend)0x918134;
#endif
        result=blend(destination,source,weights);
        if(result!=expected_result)return 1+phase;
        if(!compare())return 3+phase;
    }
    ++cases;return 0;
}
static int run(void)
{
    uint32_t i,error;
    for(i=0;i<COUNT(bases);++i)if(!map(bases[i],0x1000))return 1;
    if(!map(0x900000,0x40000)||!load("build/icestar.x",0x900000))return 2;
#ifdef KERNEL_LINKED_TEST
    if(!map(0x800000,0x40000)||!load("build/openicestaros_kernel.x",0x800000))return 3;
#endif
    for(pattern=0;pattern<2;++pattern)for(alignment=0;alignment<4;++alignment)
    for(alias=0;alias<=4*COUNT(offsets);++alias)
    for(which=0;which<COUNT(values);++which)for(mode=0;mode<2;++mode){
        error=exercise();if(error){
            PRINT("Surface alpha failure error/pattern/alignment/alias/value/mode: ");
            number(error);PRINT("/");number(pattern);PRINT("/");number(alignment);
            PRINT("/");number(alias);PRINT("/");number(which);PRINT("/");number(mode);PRINT("\n");
            return 10+error;
        }
    }
    if(coverage!=15){PRINT("Surface alpha missing coverage: ");number(coverage);PRINT("\n");return 20;}
    PRINT("Four-pixel blend original/C/oracle cases: ");number(cases);
    PRINT("; signed weights, RGB order, alpha preservation, volatile weight/pixel aliases, wrapped arithmetic, raw returns and full memory verified\n");
    return 0;
}
void _start(void){syscall3(1,run(),0,0);__builtin_unreachable();}
