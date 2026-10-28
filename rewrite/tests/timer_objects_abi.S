.text
.globl timer_checked_call
.type timer_checked_call, @function
timer_checked_call:
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    movl 32(%esp), %eax
    movl %eax, timer_checked_output
    movl 20(%esp), %eax
    movl 24(%esp), %ecx
    movl 28(%esp), %edx
    pushl $0x13579bdf
    pushl $0x2468ace0
    pushl %edx
    pushl %ecx
    movl %esp, timer_checked_stack
    movl $0x11223344, %ebp
    movl $0x55667788, %ebx
    movl $0x99aabbcc, %esi
    movl $0xddeeff00, %edi
    call *%eax
    movl timer_checked_output, %edx
    movl %eax, (%edx)
    cmpl timer_checked_stack, %esp
    jne 1f
    cmpl $0x2468ace0, 8(%esp)
    jne 1f
    cmpl $0x13579bdf, 12(%esp)
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
2:  movl timer_checked_stack, %esp
    addl $16, %esp
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret
.size timer_checked_call, .-timer_checked_call
.bss
.align 4
timer_checked_stack: .long 0
timer_checked_output: .long 0
.section .note.GNU-stack,"",@progbits
