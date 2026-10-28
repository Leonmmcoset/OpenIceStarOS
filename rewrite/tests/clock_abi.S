.text
.globl clock_invoke
.type clock_invoke,@function
/* Deterministic incoming words allow a real service to change live varargs. */
clock_invoke:
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    movl %esp, clock_outer_stack
    movl 20(%esp), %eax
    movl 24(%esp), %esi
    movl 28(%esp), %edx
    movl %edx, clock_escaping
    movl $0x66ff80, %edi
    movl $12, %ecx
    cld
    rep movsl
    movl $0x13579bdf, 0x66ffb0
    movl $0x2468ace0, 0x66ffb4
    movl $0x66ff80, %esp
    movl $0x11223344, %ebp
    movl $0x55667788, %ebx
    movl $0x99aabbcc, %esi
    movl $0xddeeff00, %edi
    call *%eax
    movl %eax, clock_result
    cmpl $0x13579bdf, 0x66ffb0
    jne clock_bad
    cmpl $0x2468ace0, 0x66ffb4
    jne clock_bad
    cmpl $0x66ff80, %esp
    jne clock_bad
    cmpl $0x11223344, %ebp
    jne clock_bad
    cmpl $0x55667788, %ebx
    jne clock_bad
    cmpl $0x99aabbcc, %esi
    jne clock_bad
    cmpl $0xddeeff00, %edi
    jne clock_bad
    jmp clock_done
clock_bad:
    movl $1, clock_bad_abi
.globl clock_finish
clock_finish:
    cmpl $0, clock_escaping
    je clock_escape_bad
    cmpl $0x13579bdf, 0x66ffb0
    jne clock_escape_bad
    cmpl $0x2468ace0, 0x66ffb4
    je clock_done
clock_escape_bad:
    movl $1, clock_bad_abi
clock_done:
    movl clock_outer_stack, %esp
    movl clock_result, %eax
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret
.size clock_invoke,.-clock_invoke
.bss
clock_outer_stack: .long 0
.globl clock_result,clock_bad_abi,clock_escaping
clock_result: .long 0
clock_bad_abi: .long 0
clock_escaping: .long 0
.section .note.GNU-stack,"",@progbits
