.section .multiboot,"a"
.align 4
.long 0x1badb002, 3, -(0x1badb002 + 3)
.section .text.start,"ax"
.code32
.global _start
_start:
    cli
    lgdt gdt_pointer
    ljmp $16, $flat_entry
flat_entry:
    mov $8, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov %ax, %fs
    mov %ax, %gs
    mov $0x700000, %esp
    cld
    call firmware_scan_qemu_main
1:  hlt
    jmp 1b
.section .rodata
.align 8
gdt:
    .quad 0
    .quad 0x00cf92000000ffff
    .quad 0x00cf9a000000ffff
gdt_pointer:
    .word 23
    .long gdt
.section .rebuilt,"a"
.incbin "build/icestar_c.x"
.section .original,"a"
.incbin "build/firmware_scan_original.x"
.section .note.GNU-stack,"",@progbits
