.code32
.section .stage,"ax"
/* Preserve the original stage through video setup and device discovery. */
.incbin "../IceStarOS_x86_32M.vhd", 0xa00, 0x391
    jmp stage_bridge
.incbin "../IceStarOS_x86_32M.vhd", 0xa00 + 0x396, 0x806 - 0x396

stage_bridge:
    ljmp $0x40, $stage_flat

.section .text.stage_flat,"ax"
.global stage_flat
stage_flat:
    movw $0x08, %ax
    movw %ax, %ss
    movl $0x7c00, %esp
    call stage_load_init
    testl %eax, %eax
    jnz stage_failed
    pushl $0x70000
    pushl $0x70000
    call stage_prepare_init
    addl $8, %esp
    movl $0x70000, %edi
    movw $0x38, %dx
    movw %dx, %ds
    movw %dx, %es
    movw %dx, %fs
    movw %dx, %gs
    movw %dx, %ss
    movl %eax, %esp
    ljmp *0x10(%edi)
stage_failed:
    cli
1:  hlt
    jmp 1b
