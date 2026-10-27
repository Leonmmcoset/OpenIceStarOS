.section .multiboot,"a"
.align 4
.long 0x1badb002, 3, -(0x1badb002 + 3)
.section .text.start,"ax"
.code32
.global _start
_start:
    cli
    lgdt gdt_pointer
    ljmp $16,$flat_entry
flat_entry:
    mov $8,%ax
    mov %ax,%ds
    mov %ax,%es
    mov %ax,%ss
    mov %ax,%fs
    mov %ax,%gs
    mov $0x700000,%esp
    cld
    call paging_qemu_main
1:  hlt
    jmp 1b

.global paging_invoke
.type paging_invoke,@function
paging_invoke:
    pushfl
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    pushl $0x13579bdf
    pushl $0x2468ace0
    movl %esp,paging_call_stack
    movl $0x11223344,%ebp
    movl $0x22334455,%ebx
    movl $0x33445566,%esi
    movl $0x44556677,%edi
    movl $0x55667788,%ecx
    movl $0x66778899,%edx
    movw paging_gs,%ax
    movw %ax,%gs
    movw paging_ds,%ax
    movw %ax,%ds
    pushl %ss:paging_input_flags
    popfl
    call *%ss:paging_entry
    /* SS remains flat even when the caller supplies a nonflat DS/GS. */
    pushfl
    popl %ss:paging_flags_after
    cld
    cli
    movl %eax,%ss:paging_eax_after
    movl %ebp,%ss:paging_registers
    movl %ebx,%ss:paging_registers+4
    movl %esi,%ss:paging_registers+8
    movl %edi,%ss:paging_registers+12
    movl %ecx,%ss:paging_registers+16
    movl %edx,%ss:paging_registers+20
    movl %esp,%ss:paging_stack_after
    movw %ds,%ss:paging_segments
    movw %es,%ss:paging_segments+2
    movw %fs,%ss:paging_segments+4
    movw %gs,%ss:paging_segments+6
    movw %ss,%ss:paging_segments+8
    movl %cr0,%eax
    movl %eax,%ss:paging_cr0_after
    movl %cr3,%eax
    movl %eax,%ss:paging_cr3_after
    movl %cr0,%eax
    andl $0x7fffffff,%eax
    movl %eax,%cr0
    jmp 2f
2:  movw $8,%ax
    movw %ax,%ds
    movw %ax,%gs
    cmpl $0x2468ace0,(%esp)
    sete paging_canaries
    cmpl $0x13579bdf,4(%esp)
    sete paging_canaries+1
    movl paging_call_stack,%esp
    addl $8,%esp
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    popfl
    ret
.size paging_invoke,.-paging_invoke

.section .rodata
.align 8
gdt:
    .quad 0
    .quad 0x00cf92000000ffff
    .quad 0x00cf9a000000ffff
    .quad 0x01cf92000000ffff /* selector 24: base 16 MiB */
    .quad 0x02cf92000000ffff /* selector 32: base 32 MiB */
    .quad 0x00cf92000000ffff
gdt_pointer:
    .word 47
    .long gdt
.section .rebuilt,"a"
.incbin "build/paging_fixture_candidate.x"
.section .original,"a"
.incbin "build/paging_original.x"
.section .note.GNU-stack,"",@progbits
