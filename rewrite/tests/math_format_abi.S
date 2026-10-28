.text
.globl format_checked_call
.type format_checked_call,@function
format_checked_call:
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    movl %esp, format_outer_stack
    movl 20(%esp), %eax
    movl 24(%esp), %esi
    movl 28(%esp), %edx
    movl format_argument_stack, %edi
    movl $6, %ecx
    cld
    rep movsl
    movl $0x13579bdf, (%edi)
    movl $0x2468ace0, 4(%edi)
    movl format_argument_stack, %esp
    movl %edx, format_output
    movl $0x11223344, %ebp
    movl $0x55667788, %ebx
    movl $0x99aabbcc, %esi
    movl $0xddeeff00, %edi
    call *%eax
    movl format_output, %edx
    movl %eax, (%edx)
    fnstsw 28(%edx)
    fnstcw 30(%edx)
    cmpl format_argument_stack, %esp
    jne 1f
    movl format_argument_stack, %eax
    cmpl $0x13579bdf, 24(%eax)
    jne 1f
    cmpl $0x2468ace0, 28(%eax)
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
2:  movl %eax, format_bad_abi
    leal 4(%edx), %edi
    movl format_argument_stack, %esi
    movl $6, %ecx
    cld
    rep movsl
    movl format_bad_abi, %eax
    movl format_outer_stack, %esp
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret
.size format_checked_call,.-format_checked_call
.bss
format_outer_stack: .long 0
format_output: .long 0
format_bad_abi: .long 0
.section .note.GNU-stack,"",@progbits
