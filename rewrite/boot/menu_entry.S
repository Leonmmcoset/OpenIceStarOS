.code16gcc
.section .header, "a"
.long 0x44b
.word menu_entry, 0x1a00, 0

.section .start, "ax"
.global menu_entry
menu_entry:
    calll menu_main
1:  jmp 1b
