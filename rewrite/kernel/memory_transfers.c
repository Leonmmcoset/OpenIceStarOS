/* Instruction-level transfer boundaries retain the original register, DF,
 * stack-argument, access-width and overlap contracts. Naked C definitions
 * prevent compiler prologues from changing these nonstandard interfaces. */
#include "memory_accel.h"

#define ARGS (uint32_t destination __attribute__((unused)), \
              uint32_t source_or_value __attribute__((unused)), \
              uint32_t count __attribute__((unused)))
#define TRANSFER(name, instructions) \
    __attribute__((naked, noinline)) void name ARGS \
    { __asm__ (instructions); __builtin_unreachable(); }

#define COPY_ENTER \
    "pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %edi\n\tpushl %esi\n\t" \
    "movl 16(%ebp),%ecx\n\tmovl 12(%ebp),%esi\n\tmovl 8(%ebp),%edi\n\tcld\n\t"
#define COPY_LEAVE \
    "movl %ecx,16(%ebp)\n\tmovl %esi,12(%ebp)\n\tmovl %edi,8(%ebp)\n\t" \
    "popl %esi\n\tpopl %edi\n\tpopl %ebp\n\tret"

TRANSFER(kernel_rep_copy_bytes, COPY_ENTER "rep movsb\n\t" COPY_LEAVE)
/* GAS otherwise emits 66 F3 instead of the original F3 66 prefix order. */
TRANSFER(kernel_rep_copy_words, COPY_ENTER ".byte 0xf3\n\tmovsw\n\t" COPY_LEAVE "\n\tnop")
TRANSFER(kernel_rep_copy_dwords, COPY_ENTER "rep movsl\n\t" COPY_LEAVE)

#define FILL_ENTER \
    "pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %edi\n\tsubl $4,%esp\n\t" \
    "movl 12(%ebp),%eax\n\t"
#define FILL_LEAVE \
    "movl %ecx,16(%ebp)\n\tmovl %edi,8(%ebp)\n\t" \
    "addl $4,%esp\n\tpopl %edi\n\tpopl %ebp\n\tret"

TRANSFER(kernel_rep_fill_bytes, FILL_ENTER
    "movb %al,-5(%ebp)\n\tmovl 16(%ebp),%ecx\n\tmovl 8(%ebp),%edi\n\t"
    "movzbl -5(%ebp),%eax\n\tcld\n\trep stosb\n\t" FILL_LEAVE)
TRANSFER(kernel_rep_fill_words, FILL_ENTER
    "movw %ax,-6(%ebp)\n\tmovl 16(%ebp),%ecx\n\tmovl 8(%ebp),%edi\n\t"
    "movzwl -6(%ebp),%eax\n\tcld\n\t.byte 0xf3\n\tstosw\n\t" FILL_LEAVE)
TRANSFER(kernel_rep_fill_dwords,
    "pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %edi\n\t"
    "movl 16(%ebp),%ecx\n\tmovl 8(%ebp),%edi\n\tmovl 12(%ebp),%eax\n\t"
    "cld\n\trep stosl\n\tmovl %ecx,16(%ebp)\n\tmovl %edi,8(%ebp)\n\t"
    "popl %edi\n\tpopl %ebp\n\tret\n\tnop")

#define SSE_ENTER \
    "pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %esi\n\tpushl %edi\n\tpushl %ecx\n\t" \
    "movl 8(%ebp),%edi\n\tmovl 12(%ebp),%esi\n\tmovl 16(%ebp),%ecx\n"
#define SSE_LEAVE "popl %ecx\n\tpopl %edi\n\tpopl %esi\n\tleave\n\tret"

TRANSFER(kernel_copy_sse16_blocks, SSE_ENTER
    "1:\tmovups (%esi),%xmm0\n\tmovups %xmm0,(%edi)\n\t"
    "addl $16,%esi\n\taddl $16,%edi\n\tloop 1b\n\t" SSE_LEAVE)
TRANSFER(kernel_copy_sse64_blocks, SSE_ENTER
    "1:\tmovups (%esi),%xmm0\n\tmovups %xmm0,(%edi)\n\t"
    "movups 16(%esi),%xmm1\n\tmovups %xmm1,16(%edi)\n\t"
    "movups 32(%esi),%xmm2\n\tmovups %xmm2,32(%edi)\n\t"
    "movups 48(%esi),%xmm3\n\tmovups %xmm3,48(%edi)\n\t"
    "addl $64,%esi\n\taddl $64,%edi\n\tloop 1b\n\t" SSE_LEAVE)

#undef ARGS
#undef TRANSFER
#undef COPY_ENTER
#undef COPY_LEAVE
#undef FILL_ENTER
#undef FILL_LEAVE
#undef SSE_ENTER
#undef SSE_LEAVE
