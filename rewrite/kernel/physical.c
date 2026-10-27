#include <stdint.h>

/* Selector 8 addresses physical memory through GS. Ordinary C pointers use DS
 * and are not interchangeable with these entry points. Keep GS and EBX,
 * the caller's stack layout, partial-width EAX results, and EFLAGS exactly as
 * the original hardware boundary does. Reads deliberately clear EAX with XOR;
 * writes deliberately leave flags unchanged. */
#define PHYSICAL_PRIMITIVE(type, name, parameters, instructions) \
    __attribute__((naked, noinline)) type name parameters \
    { __asm__ (instructions); __builtin_unreachable(); }

#define PHYSICAL_ENTER \
    "pushl %gs\n\tpushl %ebx\n\tmovl $8,%eax\n\tmovl %eax,%gs\n\t" \
    "movl 12(%esp),%ebx\n\t"
#define PHYSICAL_LEAVE "popl %ebx\n\tpopl %gs\n\tret"

PHYSICAL_PRIMITIVE(void, kernel_physical_write_byte,
                   (uint32_t address __attribute__((unused)),
                    uint32_t value __attribute__((unused))),
                   PHYSICAL_ENTER "movb 16(%esp),%al\n\t"
                   "movb %al,%gs:(%ebx)\n\t" PHYSICAL_LEAVE)

PHYSICAL_PRIMITIVE(void, kernel_physical_write_word,
                   (uint32_t address __attribute__((unused)),
                    uint32_t value __attribute__((unused))),
                   PHYSICAL_ENTER "movw 16(%esp),%ax\n\t"
                   "movw %ax,%gs:(%ebx)\n\t" PHYSICAL_LEAVE)

PHYSICAL_PRIMITIVE(void, kernel_physical_write_dword,
                   (uint32_t address __attribute__((unused)),
                    uint32_t value __attribute__((unused))),
                   PHYSICAL_ENTER "movl 16(%esp),%eax\n\t"
                   "movl %eax,%gs:(%ebx)\n\t" PHYSICAL_LEAVE)

PHYSICAL_PRIMITIVE(uint32_t, kernel_physical_read_byte,
                   (uint32_t address __attribute__((unused))),
                   PHYSICAL_ENTER "xorl %eax,%eax\n\t"
                   "movb %gs:(%ebx),%al\n\t" PHYSICAL_LEAVE)

PHYSICAL_PRIMITIVE(uint32_t, kernel_physical_read_word,
                   (uint32_t address __attribute__((unused))),
                   PHYSICAL_ENTER "xorl %eax,%eax\n\t"
                   "movw %gs:(%ebx),%ax\n\t" PHYSICAL_LEAVE)

PHYSICAL_PRIMITIVE(uint32_t, kernel_physical_read_dword,
                   (uint32_t address __attribute__((unused))),
                   PHYSICAL_ENTER "xorl %eax,%eax\n\t"
                   "movl %gs:(%ebx),%eax\n\t" PHYSICAL_LEAVE)

#undef PHYSICAL_ENTER
#undef PHYSICAL_LEAVE
#undef PHYSICAL_PRIMITIVE
