.text
.globl math_checked_call
.type math_checked_call,@function
/* Capture ST0 before binary64 conversion, as well as sticky exception flags. */
math_checked_call:
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    movl %esp, math_outer_stack
    movl 20(%esp), %eax
    movl 24(%esp), %esi
    movl 28(%esp), %edx
    movl math_argument_stack, %edi
    movl $4, %ecx
    cld
    rep movsl
    movl $0x13579bdf, (%edi)
    movl $0x2468ace0, 4(%edi)
    movl math_argument_stack, %esp
    movl %edx, math_output
    movl $0x11223344, %ebp
    movl $0x55667788, %ebx
    movl $0x99aabbcc, %esi
    movl $0xddeeff00, %edi
    call *%eax
    movl math_output, %edx
    fnstsw 20(%edx)
    fnstcw 22(%edx)
    fstpt (%edx)
    fldt (%edx)
    fstpl 12(%edx)
    cmpl math_argument_stack, %esp
    jne 1f
    movl math_argument_stack, %edx
    cmpl $0x13579bdf, 16(%edx)
    jne 1f
    cmpl $0x2468ace0, 20(%edx)
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
2:  movl math_outer_stack, %esp
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret
.size math_checked_call,.-math_checked_call
.bss
math_outer_stack: .long 0
math_output: .long 0
.section .note.GNU-stack,"",@progbits
