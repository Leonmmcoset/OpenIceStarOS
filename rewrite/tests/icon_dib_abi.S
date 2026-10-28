.text
.globl icon_dib_checked_call
.type icon_dib_checked_call, @function
icon_dib_checked_call:
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    movl 20(%esp), %eax
    movl 24(%esp), %edx
    pushl $0x13579bdf
    pushl $0x2468ace0
    pushl 12(%edx)
    pushl 8(%edx)
    pushl 4(%edx)
    pushl (%edx)
    movl %esp, icon_dib_checked_stack
    movl $0x11223344, %ebp
    movl $0x55667788, %ebx
    movl $0x99aabbcc, %esi
    movl $0xddeeff00, %edi
    call *%eax
    cmpl icon_dib_checked_stack, %esp
    jne 1f
    cmpl $0x2468ace0, 16(%esp)
    jne 1f
    cmpl $0x13579bdf, 20(%esp)
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
2:  movl icon_dib_checked_stack, %esp
    addl $24, %esp
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret
.size icon_dib_checked_call, .-icon_dib_checked_call
.bss
.align 4
icon_dib_checked_stack: .long 0
.section .note.GNU-stack,"",@progbits
