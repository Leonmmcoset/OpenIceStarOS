/* Identity paging setup 0x837448..0x8374b2. Table writes explicitly use GS,
 * as in the original, independently of the caller's DS segment base. */
#include "paging.h"

static inline void store_gs(uint32_t address, uint32_t value)
{
    __asm__ volatile("movl %0,%%gs:(%1)" :: "r"(value), "r"(address) : "memory");
}

__attribute__((used, noinline)) static void build_identity_tables(void)
{
    uint32_t i;
    for (i=0; i<1024; ++i) store_gs(0x19000u+i*4u, 0x100107u+i*4096u);
    for (i=0; i<1048576; ++i) store_gs(0x100000u+i*4u, 0x107u+i*4096u);
}

/* Control registers and segment selectors require an ISA boundary. Preserve
 * the original's ECX/EDX and GS contracts and leave the final OR's flags intact.
 * GCC preserves EBP/EBX/ESI/EDI across the C table-construction helper. */
__attribute__((naked, noinline)) uint32_t kernel_init_identity_paging(void)
{
    __asm__("pushl %gs;pushl %ecx;pushl %edx;"
        "movl $8,%eax;movw %ax,%gs;call build_identity_tables;"
        "movl $0x19000,%eax;movl %eax,%cr3;"
        "movl %cr0,%eax;orl $0x80000000,%eax;movl %eax,%cr0;"
        "jmp 1f;1:nop;popl %edx;popl %ecx;popl %gs;ret");
    __builtin_unreachable();
}
