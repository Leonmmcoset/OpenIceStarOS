#include "interrupt_entries.h"

/* 0x8371a0..0x8372e8. Symbolic assembly is required at the CPU interrupt
 * boundary: preserve the original stack layout, segment saves, EOI order,
 * STI placement, callback ABI and IRET. These are all byte-exact routines.
 * In particular, no CLD or CPU error-code adjustment is added. */
#define ENTRY(name, instructions) \
    __attribute__((naked, noinline)) void kernel_##name(void) \
    { __asm__(instructions); __builtin_unreachable(); }
#define SAVE "pushl %ds;pushal;movl $8,%eax;movl %eax,%ds;"
#define RESTORE "popal;popl %ds;iret"
#define EOI_MASTER_FIRST "movb $0x20,%al;outb %al,$0x20;nop;outb %al,$0xa0;"
#define EOI_SLAVE_FIRST "movb $0x20,%al;outb %al,$0xa0;nop;outb %al,$0x20;"
#define RTC_ACK "movb $0x0c,%al;outb %al,$0x70;inb $0x71,%al;"

ENTRY(exception_entry,
      "pushal;pushl %ds;pushl %cs;pushl %es;pushl %ss;pushl %gs;"
      "movl $8,%eax;movl %eax,%ds;call *0x18600;"
      "popl %gs;popl %ss;popl %es;popl %eax;popl %ds;popal;iret")
ENTRY(spurious_entry,
      "pushl %eax;" EOI_SLAVE_FIRST "popl %eax;iret")
ENTRY(exception_fs_entry,
      "pushal;pushl %ds;pushl %cs;pushl %es;pushl %ss;pushl %fs;pushl %gs;"
      "movl $8,%eax;movl %eax,%ds;call *0x18600;"
      "popl %gs;popl %fs;popl %ss;popl %es;popl %eax;popl %ds;popal;iret")
ENTRY(device_unavailable_entry, "clts;iret")
ENTRY(timer_entry,
      SAVE EOI_MASTER_FIRST RTC_ACK "sti;call *0x18680;" RESTORE)
ENTRY(keyboard_entry,
      SAVE EOI_MASTER_FIRST "call *0x18684;" RESTORE)
ENTRY(irq5_entry,
      SAVE EOI_MASTER_FIRST "call *0x18694;" RESTORE)
ENTRY(rtc_entry,
      SAVE EOI_MASTER_FIRST RTC_ACK "call *0x186a0;" RESTORE)
ENTRY(irq9_entry,
      SAVE EOI_SLAVE_FIRST "call *0x186a4;" RESTORE)
ENTRY(irq10_entry,
      SAVE EOI_SLAVE_FIRST "call *0x186a8;" RESTORE)
ENTRY(irq11_entry,
      SAVE EOI_SLAVE_FIRST "call *0x186ac;" RESTORE)
ENTRY(irq12_entry,
      SAVE EOI_SLAVE_FIRST "call *0x186b0;" RESTORE)
ENTRY(syscall40_entry, "sti;" SAVE "call *0x18700;" RESTORE)
ENTRY(syscall41_entry,
      "sti;" SAVE "call *0x18704;movl %eax,28(%esp);" RESTORE)

#undef RTC_ACK
#undef EOI_SLAVE_FIRST
#undef EOI_MASTER_FIRST
#undef RESTORE
#undef SAVE
#undef ENTRY
