#include "interrupts.h"

/* These hardware boundaries preserve the original non-C register/flag ABI,
 * segment changes, instruction order and I/O delays. All five are byte exact.
 * No vector bounds checks or configuration corrections are introduced. */
#define LEAF(type, name, parameters, instructions) \
    __attribute__((naked, noinline)) type name parameters \
    { __asm__(instructions); __builtin_unreachable(); }
LEAF(void, kernel_install_interrupt_gate,
     (uint32_t handler __attribute__((unused)), uint32_t selector __attribute__((unused)),
      uint32_t attributes __attribute__((unused)), uint32_t vector __attribute__((unused))),
     "pushl %ebp;movl %esp,%ebp;pushl %eax;pushl %ebx;pushl %ecx;pushl %edx;pushl %esi;"
     "movl 8(%ebp),%eax;movl 12(%ebp),%ebx;movl 16(%ebp),%ecx;movl 20(%ebp),%esi;"
     "movl %eax,%edx;andl $0xffff0000,%edx;orw %cx,%dx;andl $0xffff,%eax;"
     "shll $16,%ebx;orl %ebx,%eax;pushl %ds;movl $8,%ebx;movl %ebx,%ds;"
     "movl $0x17e00,%ebx;movl %eax,(%ebx,%esi,8);movl %edx,4(%ebx,%esi,8);"
     "movw $0x7ff,0x1a82c;movl $0x17e00,0x1a82e;lidt 0x1a82c;"
     "popl %ds;popl %esi;popl %edx;popl %ecx;popl %ebx;popl %eax;leave;ret")
LEAF(uint32_t, kernel_boot_code_selector, (void),
     "pushl %gs;movl $8,%eax;movl %eax,%gs;movl %gs:0x70014,%eax;popl %gs;ret")
LEAF(void, kernel_init_pic, (void),
     "movb $0xff,%al;outb %al,$0x21;nop;outb %al,$0xa1;"
     "movb $0x11,%al;outb %al,$0x20;nop;"
     "movb $0x20,%al;outb %al,$0x21;nop;"
     "movb $4,%al;outb %al,$0x21;nop;"
     "movb $1,%al;outb %al,$0x21;nop;"
     "movb $0x11,%al;outb %al,$0xa0;nop;"
     "movb $0x28,%al;outb %al,$0xa1;nop;"
     "movb $2,%al;outb %al,$0xa1;nop;"
     "movb $1,%al;outb %al,$0xa1;nop;"
     "movb $0xfb,%al;outb %al,$0x21;nop;"
     "movb $0xff,%al;outb %al,$0xa1;ret")
LEAF(void, kernel_init_pit, (void),
     "movb $0x34,%al;outb %al,$0x43;"
     "movb $0x9c,%al;outb %al,$0x40;"
     "movb $0x2e,%al;outb %al,$0x40;ret")
LEAF(void, kernel_init_rtc, (void),
     "movb $0x0b,%al;orb $0x80,%al;outb %al,$0x70;"
     "movb $0x12,%al;outb %al,$0x71;inb $0xa1,%al;"
     "andb $0xfe,%al;outb %al,$0xa1;"
     "movb $0x0c,%al;outb %al,$0x70;inb $0x71,%al;ret")
#undef LEAF
