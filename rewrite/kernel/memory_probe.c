/* Segment-sensitive RAM test recovered from 0x8373ee..0x83743c.
 * Each page's last DWORD is saved, tested with two complementary patterns,
 * and restored. DS=8 selects physical memory. This hardware boundary retains
 * exact instruction bytes, including the original missing POP EDX on the
 * bounded-success exit. The normal boot caller scans to 0xffffffff and exits
 * on a failed write; correcting the other path would change original code. */
#include "memory_init.h"

__attribute__((naked, noinline))
uint32_t kernel_probe_pages(uint32_t first __attribute__((unused)),
                            uint32_t last __attribute__((unused)))
{
    __asm__ (
        "pushl %ds\n\tpushl %edi\n\tpushl %esi\n\tpushl %ebx\n\tpushl %edx\n\t"
        "movl $8,%eax\n\tmovl %eax,%ds\n\t"
        "movl $0xaa55aa55,%esi\n\tmovl $0x55aa55aa,%edi\n\t"
        "movl 24(%esp),%eax\n"
        "1:\tmovl %eax,%ebx\n\taddl $0xffc,%ebx\n\t"
        "movl (%ebx),%edx\n\tmovl %esi,(%ebx)\n\t"
        "xorl $-1,(%ebx)\n\tcmpl (%ebx),%edi\n\tjne 2f\n\t"
        "xorl $-1,(%ebx)\n\tcmpl (%ebx),%esi\n\tjne 2f\n\t"
        "movl %edx,(%ebx)\n\taddl $0x1000,%eax\n\t"
        "cmpl 28(%esp),%eax\n\tjbe 1b\n\t"
        "popl %ebx\n\tpopl %esi\n\tpopl %edi\n\tpopl %ds\n\tret\n"
        "2:\tmovl %edx,(%ebx)\n\t"
        "popl %edx\n\tpopl %ebx\n\tpopl %esi\n\tpopl %edi\n\tpopl %ds\n\tret");
    __builtin_unreachable();
}
