.code16gcc
.section .text.stage_video_bios,"ax"
.global stage_video_bios
stage_video_bios:
    pushl %ebx
    pushl %edi
    movl 12(%esp),%eax
    movl 16(%esp),%ecx
    movl %ecx,%ebx
    movw $0x700,%di
    cmpw $0x4f00,%ax
    jne 1f
    movw $0x500,%di
1:  pushw %ax
    xorw %ax,%ax
    movw %ax,%es
    popw %ax
    int $0x10
    movzwl %ax,%eax
    popl %edi
    popl %ebx
    retl

.section .text.stage_video_read,"ax"
.global stage_video_read
stage_video_read:
    movl 4(%esp),%edx
    movl 8(%esp),%ecx
    cmpl $4,%ecx
    jne 2f
    movl %es:(%edx),%eax
    retl
2:
    cmpl $2,%ecx
    jne 3f
    movzwl %es:(%edx),%eax
    retl
3:  movzbl %es:(%edx),%eax
    retl

.section .text.stage_video_write,"ax"
.global stage_video_write
stage_video_write:
    movl 4(%esp),%edx
    movl 8(%esp),%eax
    movw %ax,%es:(%edx)
    retl
.section .note.GNU-stack,"",@progbits
