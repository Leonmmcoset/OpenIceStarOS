.code32
.section .stage,"ax"
/* Preserve the original stage until its INIT.X loading call. */
.incbin "../IceStarOS_x86_32M.vhd", 0xa00, 0x46e
    jmp stage_bridge
.incbin "../IceStarOS_x86_32M.vhd", 0xa00 + 0x473, 0x806 - 0x473

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
    ljmp $0x20, $0x47d
stage_failed:
    cli
1:  hlt
    jmp 1b
