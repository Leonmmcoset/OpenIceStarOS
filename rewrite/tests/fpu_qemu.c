/* Real CR0, x87/SSE state, and #NM execution at CPL 0 in QEMU. */
#include <stdint.h>
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define BYTES(a) ((volatile uint8_t *)(uintptr_t)(a))
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
static uint8_t expected_state[512] __attribute__((aligned(16)));
static uint8_t actual_state[512] __attribute__((aligned(16)));
static uint8_t original_state[512] __attribute__((aligned(16)));
static uint8_t expected_data[4096];
static uint64_t idt[256];
static uint32_t count, function, mode, pattern, phase, initial_cr0;
static uint32_t task_index, context_mode, trapped;
uint32_t fpu_qemu_nm_entry;
extern void fpu_qemu_exception(void), fpu_qemu_nm(void);
static const uint32_t entries[] = {0x837f5f,0x837fb5,0x837ffd,0x8380c9,0x8380e1,
    0x83730a,0x837311,0x837318,0x837320,0x837327,0x83732f,0x837337,0x83733f};
static const uint32_t indices[] = {0,1,2,255,0x7fffffff,0x80000000,0xffffffff,0x40000001};
static void put(const char *s) { while (*s) { __asm__ volatile ("outb %0,$0xe9" : : "a" (*s++)); } }
static void number(uint32_t n) { char s[11]; unsigned i=10; s[i]=0; do {s[--i]='0'+n%10;n/=10;} while(n);put(s+i); }
static uint32_t read_cr0(void) { uint32_t n; __asm__ volatile("mov %%cr0,%0":"=r"(n));return n; }
static void write_cr0(uint32_t n) { __asm__ volatile("mov %0,%%cr0"::"r"(n):"memory"); }
__attribute__((noreturn)) static void exit_guest(uint32_t code)
{
    __asm__ volatile("outl %0,$0xf4"::"a"(code));
    for (;;) __asm__ volatile("hlt");
}
static void require(int condition)
{
    if (condition) return;
    put("FAIL count/function/mode/pattern/phase/index/context/trapped: ");
    number(count);put("/");number(function);put("/");number(mode);put("/");number(pattern);put("/");
    number(phase);put("/");number(task_index);put("/");number(context_mode);put("/");number(trapped);put("\n");
    exit_guest(0x11);
}
__attribute__((noinline)) static void setup(uint32_t p)
{
    uint16_t control=0x37f|((p&3)<<10);
    uint32_t mxcsr=0x1f80|((p&3)<<13),xmm[8],i;
    for(i=0;i<8;++i)xmm[i]=(p+i)*0x1234567u;
    __asm__ volatile("fninit; fldcw %0; fld1; fldpi; fldl2e; ldmxcsr %1; movups %2,%%xmm0; movups %3,%%xmm1"
        ::"m"(control),"m"(mxcsr),"m"(xmm[0]),"m"(xmm[4]):"memory");
}
static void snapshot(uint8_t *out)
{
    uint32_t i;for(i=0;i<512;++i)out[i]=0;
    __asm__ volatile("fxsave (%0)"::"r"(out):"memory");
}
static void equal(const uint8_t *a,const uint8_t *b,uint32_t size)
{
    uint32_t i;for(i=0;i<size;++i)if(a[i]!=b[i]){put("byte=");number(i);put(" ");require(0);}
}
static uint32_t address(void) { return entries[function]+(phase==0?0x100000:0); }
__attribute__((naked,noinline)) static uint32_t invoke(
    uint32_t entry __attribute__((unused)),uint32_t ptr __attribute__((unused)))
{
    __asm__("pushl %ebp;movl %esp,%ebp;pushl %ebx;pushl %esi;pushl %edi;"
        "pushl 12(%ebp);xorl %edx,%edx;call *8(%ebp);addl $4,%esp;"
        "popl %edi;popl %esi;popl %ebx;leave;ret");
    __builtin_unreachable();
}
/* Both oracle and faulting execution use this exact FLD1 address. */
__attribute__((noinline)) static void trigger(void) { __asm__ volatile("fld1":::"memory"); }
static void control_tests(void)
{
    for(function=0;function<4;++function) {
        if(function==2)continue;
        for(mode=0;mode<8;++mode)for(pattern=0;pattern<4;++pattern) {
            for(phase=0;phase<2;++phase) {
                uint32_t control=(initial_cr0&~0x2cu)|((mode&3)<<2)|((mode&4)<<3),result,expected;
                write_cr0(initial_cr0);setup(pattern);
                write_cr0(control);result=invoke(address(),0);
                expected=function==3?control|12u:(control&~12u)|(function==1?32u:0);
                require(read_cr0()==expected);
                if(function<2)require(result==1);
                write_cr0(initial_cr0);snapshot(actual_state);
                if(phase)equal(actual_state,original_state,512);
                else for(uint32_t i=0;i<512;++i)original_state[i]=actual_state[i];
                if(function<2)require(*(uint16_t *)actual_state==0x37f && *(uint16_t *)(actual_state+2)==0);
            }
            ++count;
        }
    }
    put("QEMU CPL0 FPU probe/enable/lazy original/C cases: ");number(count);put("\n");count=0;
}
static void context_tests(void)
{
    uint32_t k,i;
    for(trapped=0;trapped<2;++trapped)for(function=2;function<5;function+=2) {
        if(trapped&&function==2)continue;
        for(mode=0;mode<8;++mode)for(k=0;k<COUNT(indices);++k)
            for(context_mode=0;context_mode<3;++context_mode) {
                task_index=indices[k];
                for(phase=0;phase<2;++phase) {
                    uint32_t entry=0x600100+(k&3),control=(initial_cr0&~0x2cu)|((mode&3)<<2)|((mode&4)<<3);
                    write_cr0(initial_cr0);
                    for(i=0;i<4096;++i)BYTES(0x600000)[i]=(uint8_t)(i*37+count*19);
                    WORD(0x1b0c8)=entry-task_index*108u;WORD(0x1b0d8)=task_index;WORD(0x1b164)=0x700000;
                    WORD(entry+8)=context_mode?task_index^0x80000000:task_index;
                    WORD(entry+32)=context_mode==2?0x610000:0;
                    setup(k);snapshot((uint8_t *)0x610000);
                    setup(k^3);
                    if(context_mode==1)__asm__ volatile("fninit":::"memory");
                    if(context_mode==2)__asm__ volatile("fxrstor (%%eax)"::"a"(0x610000):"memory");
                    if(trapped)trigger();
                    snapshot(expected_state);
                    setup(k^3);write_cr0(control);
                    if(trapped){fpu_qemu_nm_entry=address();write_cr0(control|12u);trigger();}
                    else invoke(address(),0);
                    require(read_cr0()==(control&~12u));
                    require(WORD(entry+32)==(context_mode==1?0x700000+task_index*512u:context_mode==2?0x610000:0));
                    require(WORD(0x1b0c8)==entry-task_index*108u && WORD(0x1b0d8)==task_index);
                    snapshot(actual_state);equal(actual_state,expected_state,512);
                    if(!phase){for(i=0;i<4096;++i)expected_data[i]=BYTES(0x600000)[i];}
                    else equal((const uint8_t *)0x600000,expected_data,4096);
                }
                ++count;
            }
    }
    put("QEMU CPL0 task-context and real #NM original/C/oracle cases: ");number(count);put("\n");count=0;
}
static void reference(uint32_t f)
{
    switch(f){
    case 5:__asm__ volatile("fnsave (%%eax)"::"a"(0x620000):"memory");break;
    case 6:__asm__ volatile("frstor (%%eax)"::"a"(0x620000):"memory");break;
    case 7:__asm__ volatile("fstenv (%%eax)"::"a"(0x620000):"memory");break;
    case 8:__asm__ volatile("fldenv (%%eax)"::"a"(0x620000):"memory");break;
    case 9:__asm__ volatile("fxsave (%%eax)"::"a"(0x620000):"memory");break;
    case 10:__asm__ volatile("fxrstor (%%eax)"::"a"(0x620000):"memory");break;
    case 11:__asm__ volatile("xsave (%%eax)"::"a"(0x620000),"d"(0):"memory");break;
    case 12:__asm__ volatile("xrstor (%%eax)"::"a"(0x620000),"d"(0):"memory");break;
    }
}
static void leaf_tests(void)
{
    uint32_t i;
    for(function=5;function<13;++function)for(pattern=0;pattern<16;++pattern){
        for(phase=0;phase<3;++phase){
            write_cr0(initial_cr0);
            for(i=0;i<4096;++i)BYTES(0x620000)[i]=0;
            setup(pattern);
            if(function==6)reference(5);
            if(function==8)reference(7);
            if(function==10)reference(9);
            setup(pattern^3);
            if(phase==2)reference(function);else require(invoke(address(),0x620000)==0x620000);
            snapshot(actual_state);
            if(!phase){for(i=0;i<4096;++i)expected_data[i]=BYTES(0x620000)[i];for(i=0;i<512;++i)original_state[i]=actual_state[i];}
            else {equal((const uint8_t *)0x620000,expected_data,4096);equal(actual_state,original_state,512);}
        }
        ++count;
    }
    put("QEMU CPL0 eight native state leaves original/C/ISA cases: ");number(count);put("\n");
}
void fpu_qemu_main(void)
{
    uint32_t i,cr4;
    struct __attribute__((packed)){uint16_t limit;uint32_t base;} idtr={sizeof(idt)-1,(uint32_t)idt};
    for(i=0;i<256;++i){uint32_t p=(uint32_t)fpu_qemu_exception;idt[i]=(p&65535u)|0x00080000u|((uint64_t)(p&0xffff0000u)<<32)|0x8e0000000000ull;}
    {uint32_t p=(uint32_t)fpu_qemu_nm;idt[7]=(p&65535u)|0x00080000u|((uint64_t)(p&0xffff0000u)<<32)|0x8e0000000000ull;}
    __asm__ volatile("lidt %0"::"m"(idtr));
    initial_cr0=(read_cr0()|32u)&~12u;write_cr0(initial_cr0);
    __asm__ volatile("mov %%cr4,%0":"=r"(cr4));cr4|=0x40600;
    __asm__ volatile("mov %0,%%cr4; xsetbv"::"r"(cr4),"a"(3),"d"(0),"c"(0):"memory");
    put("QEMU FPU fixture at CPL0, CR4.OSFXSR/OSXSAVE enabled, XCR0=3\n");
    control_tests();context_tests();leaf_tests();put("PASS: 800 real-hardware-boundary comparisons\n");exit_guest(0x10);
}
