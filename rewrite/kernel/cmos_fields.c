/* CMOS readers at 0x82adab..0x82ae84. Named instruction source preserves
 * the hardware register ABI: all general registers except EAX survive.
 * Each read clears the NMI-disable index bit and leaves the selected index
 * in place. There is no UIP wait, binary-mode handling or rollover retry.
 * Invalid BCD nibbles are converted arithmetically, as in the original. */
#include "cmos_fields.h"

#define DECODE_BCD \
    "movb %al,%dl;andb $15,%dl;shrb $4,%al;movb $10,%dh;" \
    "mulb %dh;addb %dl,%al;"
#define FIELD(name, index) \
    __attribute__((naked, noinline)) uint32_t name(void) \
    { \
        __asm__("pushl %edx;movl $" #index ",%eax;" \
                "outb %al,$0x70;inb $0x71,%al;" DECODE_BCD "popl %edx;ret"); \
        __builtin_unreachable(); \
    }

FIELD(kernel_cmos_seconds, 0)
FIELD(kernel_cmos_minutes, 2)
FIELD(kernel_cmos_hours, 4)
FIELD(kernel_cmos_weekday, 6)
FIELD(kernel_cmos_day, 7)
FIELD(kernel_cmos_month, 8)

__attribute__((naked, noinline)) uint32_t kernel_cmos_year(void)
{
    __asm__("pushl %edx;pushl %ebx;movl $0x32,%eax;"
            "outb %al,$0x70;inb $0x71,%al;" DECODE_BCD
            "movb $100,%dh;mulb %dh;movw %ax,%bx;movl $9,%eax;"
            "outb %al,$0x70;inb $0x71,%al;" DECODE_BCD
            "addw %bx,%ax;popl %ebx;popl %edx;ret");
    __builtin_unreachable();
}
#undef FIELD
#undef DECODE_BCD
