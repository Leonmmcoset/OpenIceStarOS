#include <stdint.h>

/* ISA primitives need assembly. Keep their original stack ABI and, unlike
 * the normal C ABI, preserve EDX across every port operation. Output helpers
 * also retain the incoming upper EAX bits for byte/word writes. */

void kernel_halt(void)
{
    __asm__ volatile ("hlt" : : : "memory");
}

void kernel_disable_interrupts(void)
{
    __asm__ volatile ("cli" : : : "memory");
}

void kernel_enable_interrupts(void)
{
    __asm__ volatile ("sti" : : : "memory");
}

void kernel_delay_three_nops(void)
{
    __asm__ volatile ("nop\n\tnop\n\tnop");
}

uint32_t kernel_read_flags(void)
{
    uint32_t flags;
    __asm__ volatile ("pushfl\n\tpopl %0" : "=a" (flags) : : "memory");
    return flags;
}

__attribute__((optimize("omit-frame-pointer"))) void kernel_write_flags(uint32_t flags)
{
    __asm__ volatile ("pushl %0\n\tpopfl" : : "a" (flags) : "memory", "cc");
}

/* GCC's normal register allocation cannot express the narrow-output EAX
 * contract in these original leaf functions. Naked definitions provide just
 * that hardware boundary; all callers remain ordinary C. */
#define PORT_PRIMITIVE(type, name, parameters, instructions) \
    __attribute__((naked, noinline)) type name parameters \
    { __asm__ (instructions); __builtin_unreachable(); }

PORT_PRIMITIVE(uint32_t, kernel_read_port_byte,
               (uint32_t port __attribute__((unused))),
               "pushl %edx\n\tmovl 8(%esp),%edx\n\tmovl $0,%eax\n\t"
               "inb %dx,%al\n\tpopl %edx\n\tret")

PORT_PRIMITIVE(void, kernel_write_port_byte,
               (uint32_t port __attribute__((unused)), uint32_t value __attribute__((unused))),
               "pushl %edx\n\tmovl 8(%esp),%edx\n\tmovb 12(%esp),%al\n\t"
               "outb %al,%dx\n\tpopl %edx\n\tret")

PORT_PRIMITIVE(uint32_t, kernel_read_port_word,
               (uint32_t port __attribute__((unused))),
               "pushl %edx\n\tmovl 8(%esp),%edx\n\tmovl $0,%eax\n\t"
               "inw %dx,%ax\n\tpopl %edx\n\tret")

PORT_PRIMITIVE(void, kernel_write_port_word,
               (uint32_t port __attribute__((unused)), uint32_t value __attribute__((unused))),
               "pushl %edx\n\tmovl 8(%esp),%edx\n\tmovw 12(%esp),%ax\n\t"
               "outw %ax,%dx\n\tpopl %edx\n\tret")

PORT_PRIMITIVE(uint32_t, kernel_read_port_dword,
               (uint32_t port __attribute__((unused))),
               "pushl %edx\n\tmovl 8(%esp),%edx\n\tmovl $0,%eax\n\t"
               "inl %dx,%eax\n\tpopl %edx\n\tret")

PORT_PRIMITIVE(void, kernel_write_port_dword,
               (uint32_t port __attribute__((unused)), uint32_t value __attribute__((unused))),
               "pushl %edx\n\tmovl 8(%esp),%edx\n\tmovl 12(%esp),%eax\n\t"
               "outl %eax,%dx\n\tpopl %edx\n\tret")

PORT_PRIMITIVE(void, kernel_jump_boot_entry, (void),
               "ljmp $0x40,$0x9000\n\tret")

#undef PORT_PRIMITIVE

uint32_t kernel_read_cr0(void)
{
    uint32_t value;
    __asm__ volatile ("movl %%cr0,%0" : "=a" (value));
    return value;
}

__attribute__((optimize("omit-frame-pointer"))) void kernel_write_cr0(uint32_t value)
{
    __asm__ volatile ("movl %0,%%cr0" : : "a" (value) : "memory");
}
