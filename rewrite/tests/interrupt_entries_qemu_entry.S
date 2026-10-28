/* Real protected-mode interrupt frames, flat OS-compatible selectors. */
.section .multiboot,"a"
.align 4
.long 0x1badb002, 3, -(0x1badb002 + 3)
.section .text.start,"ax"
.code32
.global _start
_start:
    cli
    lgdt entries_gdt_pointer
    ljmp $16, $flat_entry
flat_entry:
    mov $8, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov %ax, %fs
    mov %ax, %gs
    mov $0x500000, %esp
    cld
    call interrupt_entries_qemu_main
1:  hlt
    jmp 1b
.global entries_fault
entries_fault:
    mov $'!', %al
    out %al, $0xe9
    mov $0x11, %eax
    out %eax, $0xf4
    jmp 1b
.global entries_int, entries_nm, entries_int40, entries_int41
entries_int:
    int $0xf0
    ret
entries_nm:
    fld1
    fstp %st(0)
    ret
entries_int40:
    int $0x40
    ret
entries_int41:
    int $0x41
    ret
.section .data
.align 8
.global entries_gdt
entries_gdt:
    .quad 0
    .quad 0x00cf92000000ffff /* 8: kernel data */
    .quad 0x00cf9a000000ffff /* 16: kernel code */
    .quad 0x00cf92000000ffff /* 24..56: alternate kernel data */
    .quad 0x00cf92000000ffff
    .quad 0x00cf92000000ffff
    .quad 0x00cf92000000ffff
    .quad 0x00cf92000000ffff
    .quad 0x00cffa000000ffff /* 67: user code */
    .quad 0x00cff2000000ffff /* 75: user data */
    .quad 0                  /* 80: TSS, filled by C fixture */
    .quad 0x00cff2000000ffff /* 91: alternate user data */
entries_gdt_pointer:
    .word 95
    .long entries_gdt
.section .rebuilt,"a"
.incbin "build/openicestaros_kernel.x"
.section .original,"a"
.incbin "build/icestar.x"
.section .note.GNU-stack,"",@progbits
