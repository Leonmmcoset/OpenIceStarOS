.text
.globl simd_capture
.type simd_capture,@function
/* Globals select the function, fake stack, flags, MXCSR and XMM seeds. */
simd_capture:
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    movl %esp, simd_host_stack
    movl simd_stack, %edi
    movl $simd_arguments, %esi
    movl $5, %ecx
    cld
    rep movsl
    movl simd_stack, %esp
    fninit
    fld1
    fldpi
    ldmxcsr simd_mxcsr
    movups simd_xmm_seed, %xmm0
    movups simd_xmm_seed+16, %xmm1
    movups simd_xmm_seed+32, %xmm2
    movups simd_xmm_seed+48, %xmm3
    movups simd_xmm_seed+64, %xmm4
    movups simd_xmm_seed+80, %xmm5
    movups simd_xmm_seed+96, %xmm6
    movups simd_xmm_seed+112, %xmm7
    pushl simd_flags
    popfl
    movl $0x10293847, %eax
    movl $0x21384957, %ecx
    movl $0x32495867, %edx
    movl $0x435a6977, %ebx
    movl $0x546b7a87, %ebp
    movl $0x657c8b97, %esi
    movl $0x768d9ca7, %edi
    call *simd_entry
    pushfl
    pushal
    cld
    movl %esp, %esi
    movl $simd_machine, %edi
    movl $9, %ecx
    rep movsl
    movl simd_stack, %esi
    movl $5, %ecx
    rep movsl
    fxsave simd_fxstate
    fninit
    ldmxcsr simd_default_mxcsr
    movl simd_host_stack, %esp
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret
.size simd_capture,.-simd_capture
.data
simd_default_mxcsr: .long 0x1f80
.bss
simd_host_stack: .long 0
.section .note.GNU-stack,"",@progbits
