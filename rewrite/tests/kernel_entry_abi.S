.text
.globl entry_checked_call
.type entry_checked_call, @function
entry_checked_call:
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    movl 20(%esp), %eax
    pushl $0x13579bdf
    pushl $0x2468ace0
    movl %esp, entry_checked_stack
    movl $0x11223344, %ebp
    movl $0x55667788, %ebx
    movl $0x99aabbcc, %esi
    movl $0xddeeff00, %edi
    call *%eax
    cmpl entry_checked_stack, %esp
    jne 1f
    cmpl $0x2468ace0, (%esp)
    jne 1f
    cmpl $0x13579bdf, 4(%esp)
    jne 1f
    cmpl $0x11223344, %ebp
    jne 1f
    cmpl $0x55667788, %ebx
    jne 1f
    cmpl $0x99aabbcc, %esi
    jne 1f
    cmpl $0xddeeff00, %edi
    jne 1f
    xorl %eax, %eax
    jmp 2f
1:  movl $1, %eax
2:  movl entry_checked_stack, %esp
    addl $8, %esp
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret
.size entry_checked_call, .-entry_checked_call
.bss
.align 4
entry_checked_stack: .long 0
.section .note.GNU-stack,"",@progbits
