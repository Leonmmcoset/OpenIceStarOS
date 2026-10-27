/* BIOS entry and the fixed drive-byte location required by the next stage. */
.code16gcc
.section .start, "ax"
.global _start
.global boot_drive
_start:
    jmp boot_entry
boot_drive:
    .byte 0
boot_entry:
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %ss
    movl $0x7c00, %esp
    movb %dl, boot_drive
    calll mbr_main
1:  jmp 1b
