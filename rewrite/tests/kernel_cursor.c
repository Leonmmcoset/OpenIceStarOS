/* Original instructions / recovered C / independent byte-memory model.
 * External order/refresh calls are identical observed callbacks. */
#include "../kernel/cursor.h"
#include "../kernel/surface_order.h"
#include "../kernel/surface_refresh.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define ARGS 0x630800u
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define PRINT(s) syscall3(4,1,(uint32_t)(s),sizeof(s)-1)
static const uint32_t regions[][2] = {
    {0x1b000,0x1000},{0x500000,0x1000},{0x600000,0x4000},{ARGS,0x400}
};
static const uint32_t shapes[] = {0,1,2,3,4,5,6,7,8,9,10,11,0xffffffff,0x80000000};
static const uint32_t positions[] = {0,1,2,0xffffffff,0x80000000};
static uint8_t memory[0x6400],expected[0x6400];
static uint32_t events[192],expected_events[192],event_count,expected_count;
static uint32_t path,seed,mode,layout,shape,position,modeling,unsafe,cases,skipped,mismatch;
static uint32_t call_mask,coverage,layout_cases[9];
__attribute__((used)) static uint32_t saved_stack;

static int syscall3(int n,uint32_t a,uint32_t b,uint32_t c)
{int r;__asm__ volatile("int $0x80":"=a"(r):"a"(n),"b"(a),"c"(b),"d"(c):"memory");return r;}
static void finish(uint32_t code){syscall3(1,code,0,0);__builtin_unreachable();}
static int map(uint32_t address,uint32_t size)
{uint32_t a[]={address,size,7,0x32,0xffffffff,0};return (uint32_t)syscall3(90,(uint32_t)a,0,0)==address;}
static int load(const char *name,uint32_t address)
{int f=syscall3(5,(uint32_t)name,0,0),n;if(f<0)return 0;n=syscall3(3,(uint32_t)f,address,0x40000);syscall3(6,(uint32_t)f,0,0);return n==250099;}
static void number(uint32_t n)
{char s[10];uint32_t i=10;do{s[--i]='0'+n%10;n/=10;}while(n);syscall3(4,1,(uint32_t)(s+i),10-i);}
static int offset(uint32_t address)
{
    uint32_t r,base=0;
    for(r=0;r<COUNT(regions);++r){
        if(address>=regions[r][0] && address-regions[r][0]<=regions[r][1]-4)
            return (int)(base+address-regions[r][0]);
        base+=regions[r][1];
    }
    unsafe=1;return -1;
}
static uint32_t read_word(uint32_t address)
{
    int o=offset(address);uint32_t i,result=0;
    if(o<0)return 0;
    if(!modeling)return WORD(address);
    for(i=0;i<4;++i)result|=(uint32_t)memory[o+i]<<(8*i);
    return result;
}
static void write_word(uint32_t address,uint32_t value)
{
    int o=offset(address);uint32_t i;
    if(o<0)return;
    if(!modeling){WORD(address)=value;return;}
    for(i=0;i<4;++i)memory[o+i]=(uint8_t)(value>>(8*i));
}
static void record(uint32_t value)
{if(event_count>=COUNT(events)){unsafe=1;return;}events[event_count++]=value;}
static uint32_t boundary(uint32_t kind,volatile uint32_t *a)
{
    uint32_t j,n=kind==3?5:1,v[5],d;
    if(unsafe)return 0;
    record(kind);call_mask|=1u<<kind;
    for(j=0;j<n;++j){v[j]=a[j];record(v[j]);}
    record(read_word(0x500004));record(read_word(0x1b04c));record(read_word(0x1b120));
    record(read_word(ARGS));record(read_word(ARGS+4));record(read_word(ARGS+8));
    d=read_word(0x500004);record(read_word(d+44));
    if(mode&1)write_word(0x500004,0x602000u+seed);
    if(mode&2){
        write_word(0x1b04c,0x603000u+seed);
        write_word(ARGS,(read_word(ARGS)+kind+1u)%12u);
        write_word(ARGS+4,0x600800u+4u*kind+seed);
        write_word(ARGS+8,0xfedc0000u+kind+seed);
    }
    if(mode&4){
        d=read_word(0x500004);
        write_word(d+44,kind==2?0:1);
        write_word(d+4,0x11110000u+kind);write_word(d+8,0x22220000u+kind);
        write_word(d+12,0x33330000u+kind);write_word(d+16,0x44440000u+kind);
    }
    if(mode&8){
        write_word(0x1b120,read_word(ARGS)+0x55);
        write_word(0x1b03c,0x600100u+kind+seed);write_word(0x1b11c,0x600200u+kind+seed);
        write_word(0x1b140,0x600300u+kind+seed);write_word(0x1b144,0x600400u+kind+seed);
        write_word(0x1b148,0x600500u+kind+seed);write_word(0x1b14c,0x600600u+kind+seed);
    }
    /* Changing the callback's outgoing words must not change the original
     * incoming cursor arguments unless they explicitly alias shared state. */
    if(!modeling)for(j=0;j<n;++j)a[j]=0xa5000000u+kind*16+j;
    return 0xb0000000u+kind*0x100u+v[0];
}
__attribute__((used,noinline)) static uint32_t call_boundary(uint32_t kind,volatile uint32_t *a)
{return boundary(kind,a);}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define STUB(name,id,args) \
__attribute__((naked,noinline)) uint32_t name args \
{__asm__("leal 4(%esp),%eax;pushl %eax;pushl $" #id ";call call_boundary;leal 8(%esp),%esp;ret");__builtin_unreachable();}
STUB(kernel_surface_order_append,1,(uint32_t layer))
STUB(kernel_surface_order_remove,2,(uint32_t layer))
STUB(kernel_syscall_target_81905e,3,(uint32_t layer,uint32_t x,uint32_t y,uint32_t width,uint32_t height))
#pragma GCC diagnostic pop
static uint32_t oracle_visibility(uint32_t show)
{
    uint32_t d=read_word(0x500004),p=read_word(d+44),a[5];
    if(unsafe)return 0;
    if(show?!p:p){
        a[0]=1;boundary(show?1:2,a);
        a[4]=read_word(read_word(0x500004)+16);
        a[3]=read_word(read_word(0x500004)+12);
        a[2]=read_word(read_word(0x500004)+8);
        a[1]=read_word(read_word(0x500004)+4);
        a[0]=show?1:2;return boundary(3,a);
    }
    return d;
}
static uint32_t oracle(void)
{
    uint32_t d,s,source=0;
    if(path<2)return oracle_visibility(path==0);
    d=read_word(0x1b04c);oracle_visibility(0);s=read_word(ARGS);
    if(s<10){
        if(!s)source=0x1b03c;
        else if(s==1)source=0x1b11c;
        else source=0x1b140+4*((s-2)/2);
        write_word(d,read_word(source));
    }
    write_word(0x1b120,read_word(ARGS));
    if(path==3){write_word(d+4,read_word(ARGS+4));write_word(d+8,read_word(ARGS+8));}
    return oracle_visibility(1);
}
static void initialize(void)
{
    uint32_t r,j,k=0,d=0x600000u+seed,c=d;
    modeling=unsafe=event_count=call_mask=0;
    for(r=0;r<COUNT(regions);++r)for(j=0;j<regions[r][1];++j){
        BYTE(regions[r][0]+j)=(uint8_t)((k*29u+17u*seed)^(k>>7));++k;
    }
    if(layout==1)c=0x601000u+seed;
    if(layout==2)c=ARGS+4u;
    if(layout==3)c=ARGS;
    if(layout==4)c=0x1b11c;
    if(layout==5)d=0x1b100;
    if(layout==6)c=0x500000;
    if(layout==7)d=ARGS;
    if(layout==8)c=0x1b04c;
    write_word(0x500004,d);write_word(0x1b04c,c);write_word(d+44,positions[position]);
    write_word(ARGS,shapes[shape]);write_word(ARGS+4,0x600000u+seed);
    write_word(ARGS+8,0x80000000u+seed);
    write_word(0x1b03c,0x600100u+seed);write_word(0x1b11c,0x600200u+seed);
    write_word(0x1b140,0x600300u+seed);write_word(0x1b144,0x600400u+seed);
    write_word(0x1b148,0x600500u+seed);write_word(0x1b14c,0x600600u+seed);
}
static void capture(void)
{uint32_t r,j,n=0;for(r=0;r<COUNT(regions);++r)for(j=0;j<regions[r][1];++j)memory[n++]=BYTE(regions[r][0]+j);}
__attribute__((naked,noinline)) static uint32_t invoke(uint32_t address __attribute__((unused)))
{
    __asm__("pushl %ebp;pushl %ebx;pushl %esi;pushl %edi;movl 20(%esp),%eax;"
        "movl %esp,saved_stack;movl $0x630800,%esp;call *%eax;movl saved_stack,%esp;"
        "popl %edi;popl %esi;popl %ebx;popl %ebp;ret");
    __builtin_unreachable();
}
static void jump(uint32_t source,uint32_t destination)
{BYTE(source)=0xe9;WORD(source+1)=destination-source-5;}
static uint32_t candidate(void)
{
#ifdef KERNEL_LINKED_TEST
    const uint32_t addresses[]={0x8130cc,0x81313e,0x8131b0,0x81325c};
#else
    const uint32_t addresses[]={(uint32_t)kernel_cursor_show,(uint32_t)kernel_cursor_hide,
        (uint32_t)kernel_cursor_select,(uint32_t)kernel_cursor_move};
#endif
    return addresses[path];
}
static uint32_t exercise(void)
{
    static const uint32_t original[]={0x9130cc,0x91313e,0x9131b0,0x91325c};
    uint32_t expected_return,result,p,r,j,n,mask;
    initialize();capture();modeling=1;expected_return=oracle();mask=call_mask;
    if(unsafe){++skipped;return 0;}
    expected_count=event_count;
    for(j=0;j<event_count;++j)expected_events[j]=events[j];
    for(j=0;j<sizeof(memory);++j)expected[j]=memory[j];
    for(p=0;p<2;++p){
        initialize();result=invoke(p?candidate():original[path]);
        if(unsafe||result!=expected_return||event_count!=expected_count)return 2+p;
        for(j=0;j<event_count;++j)if(events[j]!=expected_events[j]){mismatch=j;return 4+p;}
        n=0;for(r=0;r<COUNT(regions);++r)for(j=0;j<regions[r][1];++j)
            if(expected[n++]!=BYTE(regions[r][0]+j)){mismatch=regions[r][0]+j;return 6+p;}
    }
    coverage|=mask;++cases;++layout_cases[layout];return 0;
}
static int run(void)
{
    uint32_t r,p,start,error;
    for(r=0;r<COUNT(regions);++r){uint32_t a=regions[r][0]&~0xfffu;
        if(!map(a,(regions[r][0]+regions[r][1]-a+0xfffu)&~0xfffu))return 1;}
    if(!map(0x900000,0x40000)||!load("build/icestar.x",0x900000))return 2;
#ifdef KERNEL_LINKED_TEST
    if(!map(0x800000,0x40000)||!load("build/icestar_c.x",0x800000))return 3;
#endif
    for(p=0;p<2;++p){uint32_t base=p?0x800000:0x900000;
#ifndef KERNEL_LINKED_TEST
        if(p)break;
#endif
        jump(base+0x179d6,(uint32_t)kernel_surface_order_append);
        jump(base+0x178f0,(uint32_t)kernel_surface_order_remove);
        jump(base+0x1905e,(uint32_t)kernel_syscall_target_81905e);
    }
    for(path=0;path<4;++path){
        start=cases;
        for(seed=0;seed<4;++seed)for(mode=0;mode<16;++mode)
        for(layout=0;layout<9;++layout)for(shape=0;shape<(path<2?1:COUNT(shapes));++shape)
        for(position=0;position<COUNT(positions);++position){
            error=exercise();
            if(error){PRINT("Cursor failure ");number(error);PRINT(" path/seed/mode/layout/shape/position/address: ");
                number(path);PRINT("/");number(seed);PRINT("/");number(mode);PRINT("/");number(layout);
                PRINT("/");number(shapes[shape]);PRINT("/");number(positions[position]);PRINT("/");number(mismatch);PRINT("\n");return 10+error;}
        }
        PRINT("Cursor path/cases: ");number(path);PRINT("/");number(cases-start);PRINT("\n");
    }
    if(coverage!=14)return 30;
    for(r=0;r<COUNT(layout_cases);++r){if(!layout_cases[r])return 31;
        PRINT("Cursor alias layout/cases: ");number(r);PRINT("/");number(layout_cases[r]);PRINT("\n");}
    PRINT("Cursor original/C/oracle cases: ");number(cases);PRINT("; unsafe scenarios skipped: ");number(skipped);
    PRINT("\nAll mapped bytes, raw returns, callback order/arguments, captured pointer, live incoming words and outgoing isolation verified\n");
    return 0;
}
void _start(void){finish((uint32_t)run());}
