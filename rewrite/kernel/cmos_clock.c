#include "interrupt_callbacks.h"

/* 0x82ae84..0x82af08. Preserve the GS-relative hardware ABI, raw CMOS
 * bytes and exact sampling order. The original does not wait for UIP,
 * convert BCD, retry across rollover, or restore the index/NMI state. */
#define SAMPLE(index, destination) \
    "movl $" #index ",%eax;outb %al,$0x70;inb $0x71,%al;" \
    "movb %al,%gs:" #destination ";"
__attribute__((naked, noinline)) void kernel_read_cmos_clock(void)
{
    __asm__("pushl %gs;movl $8,%eax;movl %eax,%gs;"
            SAMPLE(0, 0x1b004)
            SAMPLE(2, 0x1b005)
            SAMPLE(4, 0x1b006)
            SAMPLE(6, 0x1b007)
            SAMPLE(7, 0x1b008)
            SAMPLE(8, 0x1b009)
            SAMPLE(9, 0x1b00a)
            SAMPLE(0x32, 0x1b00b)
            "popl %gs;ret");
    __builtin_unreachable();
}
#undef SAMPLE
