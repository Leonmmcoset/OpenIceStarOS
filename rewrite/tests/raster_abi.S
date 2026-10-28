#ifndef RASTER_ARGUMENT_WORDS
#define RASTER_ARGUMENT_WORDS 6
#endif
.text
.globl raster_checked_call
.type raster_checked_call, @function
/* Cdecl arguments live at 0x640f60, permitting deterministic aliases. */
raster_checked_call:
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    movl %esp, raster_outer_stack
    movl 20(%esp), %eax
    movl 24(%esp), %edx
    movl $(0x640f60+RASTER_ARGUMENT_WORDS*4+8), %esp
    pushl $0x13579bdf
    pushl $0x2468ace0
#if RASTER_ARGUMENT_WORDS == 8
    pushl 28(%edx)
    pushl 24(%edx)
#elif RASTER_ARGUMENT_WORDS != 6
#error Unsupported raster argument count
#endif
    pushl 20(%edx)
    pushl 16(%edx)
    pushl 12(%edx)
    pushl 8(%edx)
    pushl 4(%edx)
    pushl (%edx)
    movl $0x11223344, %ebp
    movl $0x55667788, %ebx
    movl $0x99aabbcc, %esi
    movl $0xddeeff00, %edi
#ifdef RASTER_INCOMING_EAX
    movl %eax, %ecx
    movl $RASTER_INCOMING_EAX, %eax
    call *%ecx
#else
    call *%eax
#endif
    movl %eax, raster_raw_result
    cmpl $0x640f60, %esp
    jne 1f
    cmpl $0x2468ace0, (0x640f60+RASTER_ARGUMENT_WORDS*4)
    jne 1f
    cmpl $0x13579bdf, (0x640f64+RASTER_ARGUMENT_WORDS*4)
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
2:  movl raster_outer_stack, %esp
    movl 28(%esp), %edi
    movl raster_raw_result, %ecx
    movl %ecx, (%edi)
    addl $4, %edi
    movl $0x640f60, %esi
    movl $RASTER_ARGUMENT_WORDS, %ecx
    cld
    rep movsl
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret
.size raster_checked_call, .-raster_checked_call
.bss
.align 4
raster_outer_stack: .long 0
raster_raw_result: .long 0
.section .note.GNU-stack,"",@progbits
