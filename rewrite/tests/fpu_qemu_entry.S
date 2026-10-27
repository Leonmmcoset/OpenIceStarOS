/* Dedicated ring-0 regression fixture; does not modify the OS image. */
.section .multiboot,"a"
.align 4
.long 0x1badb002, 0x00000003, -(0x1badb002 + 0x00000003)
.section .text.start,"ax"
.code32
.global _start
_start:
    cli
    lgdt gdt_pointer
    ljmp $8, $flat_entry
flat_entry:
    mov $16, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov %ax, %fs
    mov %ax, %gs
    mov $0x500000, %esp
    cld
    call fpu_qemu_main
1:  hlt
    jmp 1b
.global fpu_qemu_exception
fpu_qemu_exception:
    mov $'!', %al
    out %al, $0xe9
    mov $0x11, %eax
    out %eax, $0xf4
    jmp 1b
.global fpu_qemu_nm
fpu_qemu_nm:
    pushal
    call *fpu_qemu_nm_entry
    popal
    iret
.section .rodata
.align 8
gdt:
    .quad 0
    .quad 0x00cf9a000000ffff
    .quad 0x00cf92000000ffff
gdt_pointer:
    .word 23
    .long gdt
.section .rebuilt,"a"
.incbin "build/icestar_c.x"
.section .original,"a"
.incbin "build/icestar.x"
.section .note.GNU-stack,"",@progbits
