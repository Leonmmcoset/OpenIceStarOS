.text
.globl cmos_checked_call
.type cmos_checked_call, @function
cmos_checked_call:
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    movl 20(%esp), %eax
    movl %eax, cmos_target
    movl 24(%esp), %eax
    movl %eax, cmos_output
    pushl $0x13579bdf
    pushl $0x2468ace0
    movl %esp, cmos_stack
    movl $0x11223344, %ebp
    movl $0x55667788, %ebx
    movl $0x99aabbcc, %esi
    movl $0xddeeff00, %edi
    movl $0x01234567, %ecx
    movl $0x89abcdef, %edx
    movl $0xdeadbeef, %eax
    call *cmos_target
    movl %eax, cmos_result
    cmpl cmos_stack, %esp
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
    cmpl $0x01234567, %ecx
    jne 1f
    cmpl $0x89abcdef, %edx
    jne 1f
    xorl %eax, %eax
    jmp 2f
1:  movl $1, %eax
2:  movl cmos_stack, %esp
    movl cmos_output, %edx
    movl cmos_result, %ecx
    movl %ecx, (%edx)
    addl $8, %esp
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret
.size cmos_checked_call, .-cmos_checked_call
.bss
.align 4
cmos_stack: .long 0
cmos_target: .long 0
cmos_output: .long 0
cmos_result: .long 0
.section .note.GNU-stack,"",@progbits
