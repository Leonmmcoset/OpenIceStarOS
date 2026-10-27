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
    call display_dac_qemu_main
1:  hlt
    jmp 1b

.global dac_invoke
.type dac_invoke,@function
dac_invoke:
    pushfl
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    subl $20, %esp
    movl dac_first, %eax
    movl %eax, (%esp)
    movl dac_last, %eax
    movl %eax, 4(%esp)
    movl dac_rgb, %eax
    movl %eax, 8(%esp)
    movl $0x13579bdf, 12(%esp)
    movl $0x2468ace0, 16(%esp)
    movl %esp, dac_call_stack
    movl $0x11223344, %ebp
    movl $0x55667788, %ebx
    movl $0x99aabbcc, %esi
    movl $0xddeeff00, %edi
    /* DF is zero while C callbacks run; the tested POPFL sets its final value. */
    movl dac_flags, %eax
    andl $0x200, %eax
    orl $2, %eax
    pushl %eax
    popfl
    call *dac_entry
    pushfl
    popl dac_flags_after
    cld
    cli
    movl %esp, dac_stack_after
    movl %ebp, dac_registers
    movl %ebx, dac_registers+4
    movl %esi, dac_registers+8
    movl %edi, dac_registers+12
    movl $0, dac_aborted
    jmp dac_finish
.global dac_abort
.type dac_abort,@function
dac_abort:
    /* A controlled fixture escape checks the prefix of ranges that wrap
     * through INT_MAX; it never modifies a production kernel or disk. */
    cli
    cld
    movl dac_call_stack, %esp
    movl $1, dac_aborted
dac_finish:
    movl dac_call_stack, %esp
    movl (%esp), %eax
    movl %eax, dac_arguments_after
    movl 4(%esp), %eax
    movl %eax, dac_arguments_after+4
    movl 8(%esp), %eax
    movl %eax, dac_arguments_after+8
    movl 12(%esp), %eax
    movl %eax, dac_arguments_after+12
    movl 16(%esp), %eax
    movl %eax, dac_arguments_after+16
    addl $20, %esp
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    popfl
    ret
.size dac_invoke,.-dac_invoke

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
.incbin "build/display_dac_fixture_candidate.x"
.section .original,"a"
.incbin "build/display_dac_original.x"
.section .note.GNU-stack,"",@progbits
