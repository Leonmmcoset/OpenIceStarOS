.section .multiboot,"a"
.align 4
.long 0x1badb002, 0, -(0x1badb002)
.section .text.entry,"ax"
.globl _start
_start:
    cli
    cld
    lgdt fixture_gdt_pointer
    ljmp $8, $1f
1:  movw $16, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw %ax, %fs
    movw %ax, %gs
    movl $fixture_stack_top, %esp
    movb $255, %al
    outb %al, $0x21
    outb %al, $0xa1
    call timer_flags_main
2:  cli
    hlt
    jmp 2b
.section .rodata,"a"
.align 8
fixture_gdt:
    .quad 0, 0x00cf9a000000ffff, 0x00cf92000000ffff
fixture_gdt_pointer:
    .word 23
    .long fixture_gdt
.section .bss,"aw",@nobits
.align 16
fixture_stack: .skip 32768
fixture_stack_top:
.section .candidate,"awx"
.incbin "build/timer_flags_fixture_candidate.x"
.section .original,"awx"
.incbin "build/timer_objects_original.x"
.section .note.GNU-stack,"",@progbits
