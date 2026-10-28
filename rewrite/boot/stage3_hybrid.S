/* Source-built third stage. The filename retains the old image interface;
 * no original boot instructions or data are included by this object. */
.section .stage,"ax"
.global stage_header, stage_real_entry, stage_flat, stage_cpu_cpuid
stage_header:
    .long 4096
    .word stage_real_entry - stage_header, 0x1a80, 0
    .long 0x101                      /* requested VBE mode, menu may set 0x105 */
    .org 0x26
    .word 0x57                       /* GDTR: eleven descriptors */
    .long 0x7e00
    .word 0x7ff                      /* retained IDTR boot fields */
    .long 0x17e00
    .org 0x56
    .word 0, 0x105                   /* found flag and fallback mode */
    .org 0x70
    .ascii "INIT    X  "

.code16gcc
stage_real_entry:
    cli
    xorw %ax, %ax
    movw %ax, %ss
    movl $0x7c00, %esp               /* GCC needs a known 32-bit stack */
    movw $0x1a80, %ax
    movw %ax, %ds
    sti
    sidtw 0x62
    calll stage_video_select
    cld
    xorw %ax, %ax
    movw %ax, %es
    movw $(stage_gdt - stage_header), %si
    movw $0x7e00, %di
    movw $44, %cx
    rep movsw
    lgdtw 0x26
    inb $0x92, %al
    orb $2, %al
    outb %al, $0x92                  /* fast A20 gate */
    cli
    movl %cr0, %eax
    orl $1, %eax
    movl %eax, %cr0
    ljmpl $0x40, $stage_flat

/* Original eleven descriptor contracts. Selector 0x10 is replaced by the
 * VBE framebuffer descriptor in protected mode; 0x20 remains stage-relative.
 * 0x38/0x40 are flat data/code, 0x48/0x50 are their ring-three versions. */
stage_gdt:
    .long 0, 0
    .long 0x0000ffff, 0x00cf9200
    .long 0x0000ffff, 0x0040920a
    .long 0x7c00fffe, 0x00cf9600
    .long 0xa800ffff, 0x00409801
    .long 0x0000ffff, 0x00009200
    .long 0x0000ffff, 0x00009800
    .long 0x0000ffff, 0x00cf9200
    .long 0x0000ffff, 0x00cf9800
    .long 0x0000ffff, 0x00cff200
    .long 0x0000ffff, 0x00cff800

.code32
stage_flat:
    movw $0x08, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movl $0x7c00, %esp
    finit
    movl $1, %eax
stage_cpu_cpuid:
    cpuid
    movl %edx, 0x1a868
    movl %ecx, 0x1a86c
    testl $0x02000000, %edx
    jz 1f
    movl %cr0, %eax
    andw $0xfffb, %ax
    orw $2, %ax
    movl %eax, %cr0
    movl %cr4, %eax
    orw $0x600, %ax
    movl %eax, %cr4
1:
    call stage_prepare_display
    call stage_scan_pci
    call stage_load_init
    testl %eax, %eax
    jnz stage_failed
#ifdef REWRITE_INIT
    call stage_load_rewritten_init
    testl %eax, %eax
    jnz stage_failed
#endif
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
.section .note.GNU-stack,"",@progbits
