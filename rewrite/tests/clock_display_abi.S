.text
.globl clock_checked_call
.type clock_checked_call, @function
clock_checked_call:
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    movl 20(%esp), %eax
    movl 24(%esp), %edx
    pushl $0x13579bdf
    pushl $0x2468ace0
    pushl 16(%edx)
    pushl 12(%edx)
    pushl 8(%edx)
    pushl 4(%edx)
    pushl (%edx)
    movl %esp, clock_checked_stack
    movl $0x11223344, %ebp
    movl $0x55667788, %ebx
    movl $0x99aabbcc, %esi
    movl $0xddeeff00, %edi
    call *%eax
    cmpl clock_checked_stack, %esp
    jne 1f
    cmpl $0x2468ace0, 20(%esp)
    jne 1f
    cmpl $0x13579bdf, 24(%esp)
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
2:  movl clock_checked_stack, %esp
    addl $28, %esp
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret
.size clock_checked_call, .-clock_checked_call
.bss
.align 4
clock_checked_stack: .long 0
.section .note.GNU-stack,"",@progbits
