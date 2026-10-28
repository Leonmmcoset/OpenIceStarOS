#include <stdint.h>
#include "../kernel/math_format.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(a))
struct result { uint32_t result,args[6]; uint16_t status,cw; };
extern unsigned format_checked_call(uint32_t,const uint32_t *,struct result *);
uint32_t format_argument_stack;
static unsigned cases,chains,executions,precision_mode,rounding,seed,alignment;
static uint32_t current_args[6],expected_table[320];
static uint8_t expected_bytes[0x400],expected_arguments[128];
static int sys3(unsigned n, unsigned a, unsigned b, unsigned c)
{
    int result;
    __asm__ volatile ("int $0x80" : "=a"(result) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
    return result;
}
static void print(const char *text)
{
    unsigned n = 0;
    while (text[n]) ++n;
    sys3(4, 1, (unsigned)text, n);
}
static void number(uint32_t n)
{
    char text[12];
    unsigned i = sizeof(text);
    text[--i] = 0;
    do { text[--i] = (char)('0' + n % 10); n /= 10; } while (n);
    print(text + i);
}
static void fail(const char *text)
{
    print(text); print("\n"); sys3(1, 200, 0, 0);
}
static int map(uint32_t address, unsigned size)
{
    uint32_t args[] = {address, size, 7, 0x32, 0xffffffff, 0};
    return sys3(90, (unsigned)args, 0, 0) == (int)address;
}
static int read_file(const char *path,uint32_t base,unsigned size)
{
    int fd=sys3(5,(unsigned)path,0,0),n;
    if(fd<0) return 0;
    n=sys3(3,(unsigned)fd,base,size);
    sys3(6,(unsigned)fd,0,0);
    return n==(int)size;
}
static uint32_t rng_state=0x12345678;
static uint32_t random_word(void)
{
    rng_state^=rng_state<<13;rng_state^=rng_state>>17;rng_state^=rng_state<<5;return rng_state;
}
static int equal(const uint8_t *a,const uint8_t *b,unsigned n)
{
    for(unsigned i=0;i<n;++i)if(a[i]!=b[i])return 0;
    return 1;
}
static void show_result(const struct result *r)
{
    print("end ");number(r->result);print(" flags ");number(r->status&0x3f);print(" words ");
    for(unsigned i=0;i<6;++i){number(r->args[i]);print(" ");}print("\n");
}
static int exercise(const uint32_t *args,unsigned chain,const char *oracle)
{
    static const unsigned pcs[]={0,0x200,0x300};
    uint16_t cw=(uint16_t)(0x7f|pcs[precision_mode]|rounding*0x400);
    struct result expected={0},actual={0};
    unsigned i,copy;
    for(i=0;i<6;++i)current_args[i]=args[i];
    for(copy=0;copy<3;++copy) {
        if(chain&&copy==1)continue;
        if(copy==0&&!read_file("build/math_format_original.x",0x800000,250099))fail("format original kernel read");
        if(copy==2&&!read_file("build/math_format_candidate.x",0x800000,250099))fail("format candidate kernel read");
        uint32_t target=copy==1?(uint32_t)kernel_format_float_81a634:0x81a634;
        if(chain) {
            WORD(0x1b0e4)=0x8300000;WORD(0x1b034)=0x1c000;
            WORD(0x1c024)=seed&1?8:32;WORD(0x1a868)=seed&2?1u<<25:0;
            for(i=0;i<320;++i)WORD(0x8300000+i*4)=i*0x87654321u;
            ((void (*)(void))0x8005aa)();
            if(WORD(WORD(0x1b0e4)+254*4)!=0x81a634)fail("format service target");
            target=WORD(WORD(0x1b0e4)+254*4);
            for(i=0;i<320;++i) {
                if(!copy)expected_table[i]=WORD(0x8300000+i*4);
                else if(expected_table[i]!=WORD(0x8300000+i*4))fail("format complete table mismatch");
            }
        }
        for(i=0;i<0x400;++i)((volatile uint8_t *)0x710000)[i]=(uint8_t)(i*37+seed*59);
        for(i=0;i<128;++i)((volatile uint8_t *)(format_argument_stack+32))[i]=(uint8_t)(i*41+seed*61);
        __asm__ volatile("fninit; fldcw %0"::"m"(cw));
        if(format_checked_call(target,args,&actual))fail("format cdecl/canary mismatch");
        ++executions;
        if(actual.cw!=cw||(actual.status&0x3800)!=0)fail("format x87 stack/CW mismatch");
        if(oracle) {
            unsigned n=0;while(oracle[n])++n;
            if(actual.result!=args[0]+n||!equal((uint8_t *)args[0],(const uint8_t *)oracle,n)||
               *(uint8_t *)(args[0]+n)!=(uint8_t)((args[0]+n-0x710000)*37+seed*59))fail("format independent bytes/end/no-NUL oracle");
        }
        if(!copy) {
            expected=actual;
            for(i=0;i<0x400;++i)expected_bytes[i]=((volatile uint8_t *)0x710000)[i];
            for(i=0;i<128;++i)expected_arguments[i]=((volatile uint8_t *)(format_argument_stack+32))[i];
        } else {
            if(actual.result!=expected.result||!equal((uint8_t *)actual.args,(uint8_t *)expected.args,24)||
               (actual.status&0x3f)!=(expected.status&0x3f)) {
                print("copy ");number(copy);print(" expected ");show_result(&expected);
                print("actual ");show_result(&actual);return 0;
            }
            for(i=0;i<0x400;++i)if(expected_bytes[i]!=((volatile uint8_t *)0x710000)[i]) {
                print("format output byte ");number(i);return 0;
            }
            for(i=0;i<128;++i)if(expected_arguments[i]!=((volatile uint8_t *)(format_argument_stack+32))[i]) {
                print("format incoming tail byte ");number(i);return 0;
            }
        }
    }
    ++cases;if(chain)++chains;return 1;
}
static int run(void)
{
    static const unsigned offsets[]={0,1,5,12};
    static const int32_t precisions[]={-3,-1,0,1,2,6,10,20};
    static const int32_t widths[]={-10,0,1,6,20,80};
    static const uint32_t values[][2]={
        {0,0},{0,0x80000000},{1,0},{1,0x80000000},{0xffffffff,0xfffff},
        {0,0x100000},{0xffffffff,0x3fefffff},{0,0x3ff00000},{1,0x3ff00000},
        {0,0xbff00000},{0,0x3fe00000},{0,0xbfe00000},{0x54442d18,0x400921fb},
        {0,0x40240000},{0,0xc0240000},{0,0x40590000},{0,0xc0590000},
        {0xffffffff,0x7fefffff},{0xffffffff,0xffefffff},{0,0x7ff00000},
        {0,0xfff00000},{0x12345678,0x7ff81234},{1,0x7ff00000},
        {0x80000000,0x41dfffff},{0,0x41e00000},{0,0xc1e00000},
        {0,0x43e00000},{1,0x43e00000},{0,0xc3e00000},{0,0x43300000}
    };
    unsigned i,j,k,f;
    uint32_t args[6]={0};
    if(!map(0x10000,0x20000)||!map(0x680000,0x10000)||!map(0x710000,0x10000)||
       !map(0x800000,0x40000)||!map(0x8300000,0x10000))return 1;
    for(alignment=0;alignment<4;++alignment) {
        format_argument_stack=0x68fe80+offsets[alignment];
        for(precision_mode=0;precision_mode<3;++precision_mode)for(rounding=0;rounding<4;++rounding) {
            args[0]=0x710080+offsets[alignment];
            for(f=0;f<256;++f)for(i=0;i<2;++i)for(j=0;j<2;++j)for(k=0;k<2;++k) {
                union {double d;uint32_t w[2];} u={i?-1.05:0.0};
                seed=f+i+j+k;args[1]=u.w[0];args[2]=u.w[1];args[3]=k?12:0;args[4]=j?2:0;args[5]=f;
                if(!exercise(args,0,0))goto mismatch;
            }
            for(i=0;i<sizeof(values)/sizeof(values[0]);++i)
                for(j=0;j<sizeof(precisions)/4;++j)for(k=0;k<sizeof(widths)/4;++k) {
                    seed=i+j+k;args[1]=values[i][0];args[2]=values[i][1];args[3]=(uint32_t)widths[k];
                    args[4]=(uint32_t)precisions[j];args[5]=random_word();
                    if(!exercise(args,0,0))goto mismatch;
                }
            for(i=0;i<256;++i) {
                seed=i;args[1]=random_word();args[2]=random_word();args[3]=random_word()%80u-20;
                args[4]=random_word()%24u-3;args[5]=random_word();
                if(!exercise(args,1,0))goto mismatch;
            }
            /* Oracle cases expose original quirks independent of either code:
               fractional zeros append on the right; '.' is unconditional;
               zeros precede the sign; no terminating NUL is written. */
            union {double d;uint32_t w[2];} u={1.05};
            args[1]=u.w[0];args[2]=u.w[1];args[3]=0;args[4]=2;args[5]=128;seed=0;
            if(precision_mode==2&&rounding==0&&!exercise(args,0,"1.50"))goto mismatch;
            u.d=-2.0;args[1]=u.w[0];args[2]=u.w[1];args[3]=8;args[4]=0;args[5]=129;
            if(!exercise(args,0,"00000-2."))goto mismatch;
            args[3]=0;args[4]=0;args[5]=128;
            if(!exercise(args,0,"-2."))goto mismatch;
            /* A store onto the incoming width changes its final live value.
               Precision zero keeps this controlled alias inside that DWORD. */
            args[0]=format_argument_stack+12;args[1]=0;args[2]=0x3ff00000;
            args[3]=0;args[4]=0;args[5]=2;
            if(!exercise(args,0,0))goto mismatch;
        }
    }
    print("float formatter verified: ");number(cases);print(" scenarios / ");number(executions);
    print(" executions, ");number(chains);print(" service scenarios\n");return 0;
mismatch:
    print("\nformatter mismatch PC ");number(precision_mode);print(" rounding ");number(rounding);
    print(" alignment ");number(alignment);print(" args ");
    for(i=0;i<6;++i){number(current_args[i]);print(" ");}print("\n");return 10;
}
void _start(void){sys3(1,(unsigned)run(),0,0);__builtin_unreachable();}
