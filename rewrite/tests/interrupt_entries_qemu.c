/* Native INT/IRET, #NM and CPL3->CPL0->CPL3 differential execution. */
#include <stdint.h>
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define USED __attribute__((used))
#define ENTRY(name, start, end) start,
static const uint32_t entries[]={
#include "../kernel/interrupt_entries.def"
};
#undef ENTRY
static const uint32_t slots[]={0,0xffffffff,0,0xffffffff,32,33,37,40,41,42,43,44,64,65};
static uint64_t idt[256] __attribute__((aligned(8)));
static uint32_t tss[26];
static uint32_t original[14],original_callback[14],count,group,function,pattern,phase,gate_type;
uint32_t entries_input[8],entries_result[14],entries_callback_result[14];
uint32_t entries_flags,entries_callback_flags,entries_return,entries_hits,entries_clobber;
uint32_t entries_segment,entries_changed_segment,entries_stack,entries_operation,entries_kernel_stack;
extern uint64_t entries_gdt[];
extern void entries_fault(void),entries_int(void),entries_nm(void),entries_int40(void),entries_int41(void);
static void put(const char *s){while(*s)__asm__ volatile("outb %0,$0xe9"::"a"(*s++));}
static void number(uint32_t n){char s[11];uint32_t i=10;s[i]=0;do{s[--i]='0'+n%10;n/=10;}while(n);put(s+i);}
__attribute__((noreturn)) static void done(uint32_t n)
{__asm__ volatile("outl %0,$0xf4"::"a"(n));for(;;)__asm__ volatile("hlt");}
static void require_at(int condition,uint32_t line)
{
    if(condition)return;
    put("FAIL count/group/function/pattern/phase/gate: ");number(count);put("/");number(group);put("/");
    number(function);put("/");number(pattern);put("/");number(phase);put("/");number(gate_type);
    put(" line=");number(line);put("\n");done(0x11);
}
#define require(condition) require_at((condition),__LINE__)
static void out(uint16_t port,uint8_t value){__asm__ volatile("outb %0,%w1"::"a"(value),"d"(port));}
static uint8_t in(uint16_t port){uint8_t value;__asm__ volatile("inb %w1,%0":"=a"(value):"d"(port));return value;}
static uint32_t cr0(void){uint32_t n;__asm__ volatile("mov %%cr0,%0":"=r"(n));return n;}
static void cr0_set(uint32_t n){__asm__ volatile("mov %0,%%cr0"::"r"(n):"memory");}
static void gate(uint32_t vector,uint32_t entry,uint32_t attributes)
{idt[vector]=(entry&0xffffu)|((uint64_t)16<<16)|((uint64_t)attributes<<32)|((uint64_t)(entry&0xffff0000u)<<32);}

#define SNAPSHOT(destination) \
    "pushfl;pushal;movl %esp,%esi;movl $" destination ",%edi;movl $9,%ecx;cld;rep movsl;" \
    "xorl %eax,%eax;movw %ds,%ax;movl %eax," destination "+36;" \
    "movw %es,%ax;movl %eax," destination "+40;movw %ss,%ax;movl %eax," destination "+44;" \
    "movw %fs,%ax;movl %eax," destination "+48;movw %gs,%ax;movl %eax," destination "+52;addl $36,%esp;"
#define INPUT \
    "movl entries_input,%edi;movl entries_input+4,%esi;movl entries_input+8,%ebp;" \
    "movl entries_input+16,%ebx;movl entries_input+20,%edx;movl entries_input+24,%ecx;movl entries_input+28,%eax;"
#define SEGMENTS \
    "movl entries_segment,%eax;movw %ax,%ds;movw %ax,%es;movw %ax,%fs;movw %ax,%gs;"
#define RESET_SEGMENTS \
    "movl $8,%eax;movw %ax,%ds;movw %ax,%es;movw %ax,%fs;movw %ax,%gs;"
__attribute__((naked,noinline)) static USED void callback(void)
{
    __asm__(SNAPSHOT("entries_callback_result")
        "incl entries_hits;cmpl $0,entries_clobber;je 1f;"
        "movl entries_changed_segment,%eax;movw %ax,%ds;movw %ax,%es;movw %ax,%fs;movw %ax,%gs;1:"
        "movl $0xabc123,%ebp;movl $0xbcd234,%ebx;movl $0xcde345,%esi;movl $0xdef456,%edi;"
        "movl $0x123abc,%ecx;movl $0x234bcd,%edx;movl entries_return,%eax;"
        "pushl entries_callback_flags;popfl;ret");
    __builtin_unreachable();
}
__attribute__((naked,noinline)) static void invoke(void)
{
    __asm__("pushl %ebp;pushl %ebx;pushl %esi;pushl %edi;"
        SEGMENTS "movl %esp,entries_stack;pushl entries_flags;popfl;" INPUT
        "call *entries_operation;" SNAPSHOT("entries_result")
        "cli;cld;" RESET_SEGMENTS "popl %edi;popl %esi;popl %ebx;popl %ebp;ret");
    __builtin_unreachable();
}
__attribute__((naked,noinline)) static void user_return(void)
{
    __asm__("cli;cld;" RESET_SEGMENTS
        "movl entries_kernel_stack,%esp;popl %edi;popl %esi;popl %ebx;popl %ebp;ret");
    __builtin_unreachable();
}
__attribute__((naked,noinline)) static USED void user_code(void)
{
    __asm__(INPUT "call *entries_operation;" SNAPSHOT("entries_result") "int $0xf1;ud2");
    __builtin_unreachable();
}
__attribute__((naked,noinline)) static void invoke_user(void)
{
    __asm__("pushl %ebp;pushl %ebx;pushl %esi;pushl %edi;movl %esp,entries_kernel_stack;"
        SEGMENTS "movl $0x520000,entries_stack;"
        "pushl $75;pushl $0x520000;pushl entries_flags;pushl $67;pushl $user_code;iret");
    __builtin_unreachable();
}
static void setup(void)
{
    uint32_t i;
    for(i=0;i<8;++i)entries_input[i]=0x10293847u*(i+1)^(pattern*0x10204081u);
    for(i=0;i<14;++i)entries_result[i]=entries_callback_result[i]=0;
    for(i=0;i<256;++i)WORD(0x18600+i*4)=(uint32_t)entries_fault;
    if(slots[function]!=0xffffffff)WORD(0x18600+slots[function]*4)=(uint32_t)callback;
    entries_flags=2|(pattern&1?0x8d5:0)|(pattern&2?0x400:0)|(pattern&4?0x200:0);
    entries_callback_flags=2|(pattern&1?0:0xcd5);
    entries_return=(pattern>>3)*0x55555555u;
    entries_hits=0;entries_clobber=pattern&8;
    entries_segment=group==3?75:(pattern&16?24:8);
    entries_changed_segment=group==3?91:56;
}
static void compare(uint32_t *data,uint32_t *saved,uint32_t size)
{
    uint32_t i;
    for(i=0;i<size;++i){if(!phase)saved[i]=data[i];else if(data[i]!=saved[i]){put("register=");number(i);put(" ");require(0);}}
}
static void check(void)
{
    uint32_t i,callback_expected=slots[function]!=0xffffffff;
    require(entries_hits==callback_expected);
    for(i=0;i<8;++i){
        uint32_t expected=i==3?entries_stack-4:entries_input[i];
        if(i==7&&function==13)expected=entries_return;
        require(entries_result[i]==expected);
    }
    require(entries_result[8]==entries_flags);
    require(entries_result[9]==entries_segment && entries_result[11]==(group==3?75u:8u));
    for(i=10;i<14;++i){
        uint32_t expected=entries_segment;
        if(i==11)expected=group==3?75u:8u;
        else if(callback_expected&&entries_clobber&&function!=2&&!(function==0&&(i==10||i==13)))
            expected=entries_changed_segment;
        require(entries_result[i]==expected);
    }
    compare(entries_result,original,COUNT(original));
    if(callback_expected){
        uint32_t flags=entries_flags;
        if(!gate_type)flags&=~0x200u;
        if(function==4||function>=12)flags|=0x200;
        require(entries_callback_result[8]==flags && entries_callback_result[9]==8);
        require(entries_callback_result[10]==entries_segment && entries_callback_result[11]==8);
        require(entries_callback_result[12]==entries_segment && entries_callback_result[13]==entries_segment);
        for(i=0;i<8;++i)if(i!=3&&i!=7)require(entries_callback_result[i]==entries_input[i]);
        if(function==0||function==2||function>=12)require(entries_callback_result[7]==8);
        else if(function!=4&&function!=7)require(entries_callback_result[7]==0x20);
        /* RTC status C is volatile between phases; all upper EAX bits remain checked. */
        if(function==4||function==7){require((entries_callback_result[7]&0xffffff00u)==0);entries_callback_result[7]&=0xffffff00u;}
        compare(entries_callback_result,original_callback,COUNT(original_callback));
    }
    require(in(0x21)==0xff && in(0xa1)==0xff);
}
void interrupt_entries_qemu_main(void)
{
    uint32_t i,initial_cr0=cr0();
    struct __attribute__((packed)){uint16_t limit;uint32_t base;} descriptor={sizeof(idt)-1,(uint32_t)idt};
    out(0x21,0xff);out(0xa1,0xff);out(0x70,0x8b);out(0x71,2);
    for(i=0;i<COUNT(idt);++i)gate(i,(uint32_t)entries_fault,0x8e00);
    __asm__ volatile("lidt %0"::"m"(descriptor));
    put("QEMU native interrupt entries, original/rebuilt/oracle\n");
    group=1;
    for(function=0;function<COUNT(entries);++function)for(gate_type=0;gate_type<2;++gate_type)
        for(pattern=0;pattern<32;++pattern){
            for(phase=0;phase<2;++phase){
                setup();gate(0xf0,entries[function]+(phase?0:0x100000),gate_type?0x8f00:0x8e00);
                cr0_set(initial_cr0|8);entries_operation=(uint32_t)entries_int;invoke();
                require(cr0()==(function==3?(initial_cr0&~8u):(initial_cr0|8)));
                cr0_set(initial_cr0);check();
            }
            ++count;
        }
    put("CPL0 INT/IRET, callbacks, flags, GPRs and segments: 896\n");
    group=2;function=3;gate_type=0;
    for(pattern=0;pattern<32;++pattern){
        for(phase=0;phase<2;++phase){
            setup();gate(7,entries[function]+(phase?0:0x100000),0x8e00);
            cr0_set(initial_cr0&~12u);__asm__ volatile("fninit");cr0_set((initial_cr0&~4u)|8);
            entries_operation=(uint32_t)entries_nm;invoke();require(cr0()==(initial_cr0&~12u));
            cr0_set(initial_cr0);check();
        }
        ++count;
    }
    put("Actual #NM, CLTS, fault retry and IRET: 32\n");
    for(i=0;i<COUNT(tss);++i)tss[i]=0;
    tss[1]=0x510000;tss[2]=8;tss[25]=sizeof(tss)<<16;
    {uint32_t base=(uint32_t)tss;entries_gdt[10]=(sizeof(tss)-1)|((uint64_t)(base&0xffffff)<<16)|
        ((uint64_t)0x89<<40)|((uint64_t)(base>>24)<<56);}
    __asm__ volatile("ltr %w0"::"r"((uint16_t)80));
    gate(0xf1,(uint32_t)user_return,0xee00);
    group=3;
    for(function=12;function<14;++function)for(gate_type=0;gate_type<2;++gate_type)
        for(pattern=0;pattern<32;++pattern){
            for(phase=0;phase<2;++phase){
                setup();gate(function==12?0x40:0x41,entries[function]+(phase?0:0x100000),gate_type?0xef00:0xee00);
                entries_operation=(uint32_t)(function==12?entries_int40:entries_int41);invoke_user();check();
            }
            ++count;
        }
    put("CPL3 syscall 0x40/0x41, TSS stack switch and user IRET: 128\n");
    require(count==1056);put("PASS: 1056 CPU interrupt-entry comparisons\n");done(0x10);
}
