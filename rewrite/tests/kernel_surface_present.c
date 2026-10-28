/* Original/C/oracle comparison of the unusual rectangle predicate and both
 * display submission formats. Only the 32-bit row copier is an observer. */
#include "../kernel/surface_present.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define PRINT(s) syscall3(4,1,(uint32_t)(s),sizeof(s)-1)
static const uint32_t starts[]={0x1b000,0x500000,0x600000,0x610000,0x620000};
static const uint32_t rectangles[][4]={
    {0,0,1,1},{2,3,7,4},{0xffffffff,0xfffffffe,9,6},{0,0,0,0},
    {3,4,0,0},{7,5,0xffffffff,0xfffffffe},{12,9,20,20},{17,11,5,4},
    {18,12,1,1},{200,300,1,1},{0x7fffffff,0,1,1},{0,0x7fffffff,1,1},
    {0x80000000,0x80000000,4,3},{1,2,0x80000000,2},{1,2,0xfffffffd,2},
    {1,2,1,0x80000000},{0,0,0xffffffff,0xffffffff},{17,11,0x7fffffff,2},
    {2,2,2,1},{2,2,3,1},{2,2,4,1},{2,2,5,1},{2,2,6,1},{2,2,8,1}};
static const uint32_t screens[][2]={
    {18,12},{1,1},{0,0},{0x80000000,10},{18,0xffffffff},
    {0x7fffffff,12},{18,0x80000000}};
static const uint32_t offsets[]={0,1,0xffffffff,4,0xfffffffc};
static const uint32_t boundaries[]={0,1,2,0x7f,0xff,0xffff,0x7ffffffe,
    0x7fffffff,0x80000000,0x80000001,0xfffffffe,0xffffffff};
static uint8_t expected[0x5000];
static uint32_t events[32][3],expected_events[32][3],expected_after[5];
static uint32_t mode,alignment,alias,rect,screen_case,depth,calls,failure;
static uint32_t present_cases,region_cases,skipped,coverage,root,output,input;
uint32_t present_arguments[4],present_after[5];
volatile uint32_t *present_caller_arguments;
static int syscall3(int n,uint32_t a,uint32_t b,uint32_t c)
{int r;__asm__ volatile("int $0x80":"=a"(r):"a"(n),"b"(a),"c"(b),"d"(c):"memory");return r;}
static int map(uint32_t address,uint32_t size)
{uint32_t a[]={address,size,7,0x32,0xffffffff,0};return (uint32_t)syscall3(90,(uint32_t)a,0,0)==address;}
static int load(const char *path,uint32_t address)
{int fd=syscall3(5,(uint32_t)path,0,0),n;if(fd<0)return 0;n=syscall3(3,(uint32_t)fd,address,0x40000);syscall3(6,(uint32_t)fd,0,0);return n==250099;}
static void number(uint32_t n)
{char s[10];uint32_t i=10;do{s[--i]='0'+n%10;n/=10;}while(n);syscall3(4,1,(uint32_t)(s+i),10-i);}
static int64_t signed_word(uint32_t n)
{return n&0x80000000u?(int64_t)n-0x100000000LL:n;}
static void effects(void)
{
    if(calls==1){
        if(mode&1){
            WORD(0x1b0e4)=0xdeadbeef;WORD(0x600101+alignment+68)=0xdeadbeef;
            WORD(0x500000)=0xdeadbeef;WORD(0x1b070)=0xdeadbeef;
            WORD(root)=0xdeadbeef;WORD(root+12)=0xdeadbeef;WORD(root+16)=0xdeadbeef;
        }
        if(mode&2){
            present_caller_arguments[0]+=1;present_caller_arguments[1]^=0x80000000;
            present_caller_arguments[2]=0x12345678;present_caller_arguments[3]=0xabcdef01;
        }
    }
    WORD(0x1b300)^=calls*0x132457;
}
__attribute__((used,noinline)) static uint32_t observe(volatile uint32_t *a)
{
    uint32_t dest=a[0],source=a[1],count=a[2],i;
    if(calls>=32||count>256){failure=1;return 0;}
    events[calls][0]=dest;events[calls][1]=source;events[calls][2]=count;
    for(i=0;i<count;++i)WORD(dest+4*i)=WORD(source+4*i);
    ++calls;effects();
    if(mode&4)for(i=0;i<3;++i)a[i]=0xa5a50000|i;
    return 0xcdef0000|calls;
}
__attribute__((naked,noinline)) static uint32_t copier(void)
{__asm__("leal 4(%esp),%eax;pushl %eax;call observe;addl $4,%esp;ret");}
__attribute__((naked,noinline)) static uint32_t invoke(uint32_t target __attribute__((unused)))
{
    __asm__("pushl %ebp;movl %esp,%ebp;subl $40,%esp;"
        "movl present_arguments,%edx;movl %edx,0(%esp);"
        "movl present_arguments+4,%edx;movl %edx,4(%esp);"
        "movl present_arguments+8,%edx;movl %edx,8(%esp);"
        "movl present_arguments+12,%edx;movl %edx,12(%esp);"
        "movl $0x93fedc71,16(%esp);leal 0(%esp),%edx;"
        "movl %edx,present_caller_arguments;call *8(%ebp);"
        "movl 0(%esp),%edx;movl %edx,present_after;"
        "movl 4(%esp),%edx;movl %edx,present_after+4;"
        "movl 8(%esp),%edx;movl %edx,present_after+8;"
        "movl 12(%esp),%edx;movl %edx,present_after+12;"
        "movl 16(%esp),%edx;movl %edx,present_after+16;leave;ret");
}
static void initialize(void)
{
    uint32_t r,i;
    for(r=0;r<COUNT(starts);++r)for(i=0;i<0x1000;++i)
        BYTE(starts[r]+i)=(uint8_t)(i*37+mode*83+r*59);
    root=0x600000+alignment;output=0x610700+alignment;
    input=alias?output+offsets[alias-1]:0x620700+alignment;
    WORD(0x500000)=root;WORD(root)=output;
    WORD(root+12)=screens[screen_case][0];WORD(root+16)=screens[screen_case][1];
    WORD(0x1b070)=input;WORD(0x1b0e4)=0x600101+alignment;
    WORD(0x600101+alignment+68)=(uint32_t)copier;
    for(i=0;i<4;++i)present_arguments[i]=rectangles[rect][i];
    calls=failure=0;for(i=0;i<96;++i)((uint32_t *)events)[i]=0;
}
static int valid(uint32_t address,uint32_t length)
{
    return length<=0x1000 &&
        ((address>=0x610000 && address<=0x611000-length)||
         (address>=0x620000 && address<=0x621000-length));
}
/* Checked independent oracle. Unsafe generated memory ranges are counted
 * and skipped before executing either machine-code implementation. */
static int oracle(uint32_t *result)
{
    uint32_t a[5],i,x,y,right,bottom,pitch=screens[screen_case][0];
    uint32_t height=screens[screen_case][1],span,row,rows=0;
    int32_t words,tail;
    for(i=0;i<4;++i)a[i]=rectangles[rect][i];
    a[4]=0x93fedc71;present_caller_arguments=a;
    if(a[0]&0x80000000)a[0]=0;
    if(a[1]&0x80000000)a[1]=0;
    if(signed_word(a[0])>=signed_word(pitch)){*result=a[0];coverage|=1;goto done;}
    if(signed_word(a[1])>=signed_word(height)){*result=a[1];coverage|=2;goto done;}
    right=a[0]+a[2]-1;bottom=a[1]+a[3]-1;
    if(signed_word(right)>=signed_word(pitch)){right=pitch-1;coverage|=4;}
    if(signed_word(bottom)>=signed_word(height)){bottom=height-1;coverage|=8;}
    x=a[0];y=a[1];
    if(signed_word(x)>signed_word(right)){a[0]=right;right=x;coverage|=16;}
    if(signed_word(y)>signed_word(bottom)){a[1]=bottom;bottom=y;coverage|=32;}
    span=right-a[0]+1;
    words=(int32_t)(signed_word(span)/4);
    if(signed_word(span)<0 && span%4) --words;
    tail=(int32_t)(signed_word(span)%4);
    for(row=a[1];signed_word(row)<=signed_word(bottom);++row){
        uint32_t offset=row*pitch+a[0],dest,source,n;
        if(++rows>24)return 0;
        if(depth){
            dest=output+4*offset;source=input+4*offset;
            if(span>256||!valid(dest,4*span)||!valid(source,4*span))return 0;
            events[calls][0]=dest;events[calls][1]=source;events[calls][2]=span;
            for(n=0;n<span;++n){uint32_t word=WORD(source+4*n);WORD(dest+4*n)=word;}
            ++calls;effects();coverage|=64;
        }else{
            dest=output+offset;source=input+offset;
            for(n=0;signed_word(n)<words;++n){
                uint32_t word;
                if(!valid(dest+4*n,4)||!valid(source+4*n,4))return 0;
                word=WORD(source+4*n);WORD(dest+4*n)=word;
            }
            for(n=0;signed_word(n)<tail;++n){
                uint32_t delta=4*(uint32_t)words+n;
                if(!valid(dest+delta,1)||!valid(source+delta,1))return 0;
                BYTE(dest+delta)=BYTE(source+delta);
            }
            coverage|=128;
        }
    }
    *result=row;
done:
    for(i=0;i<5;++i)expected_after[i]=a[i];
    return 1;
}
static void save(void)
{
    uint32_t r,i,n=0;
    for(r=0;r<COUNT(starts);++r)for(i=0;i<0x1000;++i)expected[n++]=BYTE(starts[r]+i);
    for(i=0;i<96;++i)((uint32_t *)expected_events)[i]=((uint32_t *)events)[i];
}
static int compare(void)
{
    uint32_t r,i,n=0;
    for(r=0;r<COUNT(starts);++r)for(i=0;i<0x1000;++i)
        if(expected[n++]!=BYTE(starts[r]+i))return 0;
    for(i=0;i<96;++i)if(((uint32_t *)expected_events)[i]!=((uint32_t *)events)[i])return 0;
    for(i=0;i<5;++i)if(expected_after[i]!=present_after[i])return 0;
    return 1;
}
static uint32_t candidate(void)
{
#ifdef KERNEL_LINKED_TEST
    return depth?0x818f22:0x818d5e;
#else
    return depth?(uint32_t)kernel_surface_present32:(uint32_t)kernel_surface_present8;
#endif
}
static int exercise(void)
{
    uint32_t phase,result,expected_result,expected_calls;
    initialize();if(!oracle(&expected_result)){++skipped;return 0;}save();expected_calls=calls;
    for(phase=0;phase<2;++phase){
        initialize();result=invoke(phase?candidate():(depth?0x918f22:0x918d5e));
        if(failure||result!=expected_result||calls!=expected_calls)return 1+phase;
        if(!compare())return 3+phase;
    }
    ++present_cases;return 0;
}
typedef uint32_t (*Region)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
static int regions(void)
{
    uint32_t i,j,seed=0x13579bdf,a[8],expected_value;
    Region original=(Region)0x918d14;
#ifdef KERNEL_LINKED_TEST
    Region recovered=(Region)0x818d14;
#else
    Region recovered=kernel_surface_regions_test;
#endif
    for(i=0;i<131072;++i){
        for(j=0;j<8;++j){
            seed=seed*1664525+1013904223;
            a[j]=i<65536?boundaries[((i>>(4*(j%4)))+3*j)%COUNT(boundaries)]:seed;
        }
        expected_value=1;
        if(signed_word(a[0]+a[2])<=signed_word(a[4]) &&
           signed_word(a[1]+a[3])<=signed_word(a[5]))expected_value=0;
        if(signed_word(a[4]+a[6])<=signed_word(a[0]) &&
           signed_word(a[5]+a[7])<=signed_word(a[1]))expected_value=0;
        if(original(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7])!=expected_value||
           recovered(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7])!=expected_value)return 0;
        ++region_cases;
    }
    return 1;
}
static int run(void)
{
    uint32_t r,error;
    for(r=0;r<COUNT(starts);++r)if(!map(starts[r],0x1000))return 1;
    if(!map(0x900000,0x40000)||!load("build/icestar.x",0x900000))return 2;
#ifdef KERNEL_LINKED_TEST
    if(!map(0x800000,0x40000)||!load("build/openicestaros_kernel.x",0x800000))return 3;
#endif
    if(!regions())return 4;
    PRINT("Surface region predicate original/C/oracle cases: ");number(region_cases);PRINT("\n");
    for(depth=0;depth<2;++depth)for(mode=0;mode<8;++mode)
    for(alignment=0;alignment<4;++alignment)for(alias=0;alias<6;++alias)
    for(rect=0;rect<COUNT(rectangles);++rect)for(screen_case=0;screen_case<COUNT(screens);++screen_case){
        error=exercise();if(error){
            PRINT("Surface present failure error/depth/mode/alignment/alias/rect/screen/cases: ");
            number(error);PRINT("/");number(depth);PRINT("/");number(mode);PRINT("/");number(alignment);
            PRINT("/");number(alias);PRINT("/");number(rect);PRINT("/");number(screen_case);
            PRINT("/");number(present_cases);PRINT("\n");return 10+error;
        }
    }
    if(coverage!=255)return 20;
    PRINT("Surface present original/C/oracle cases: ");number(present_cases);
    PRINT("; unsafe generated ranges skipped: ");number(skipped);
    PRINT("; 8/32-bit clipping, caller stores, aliasing, cached copier, argument mutations and full memory verified\n");
    return 0;
}
void _start(void){syscall3(1,run(),0,0);__builtin_unreachable();}
