/* Four vector instruction boundaries at 0x81ab34..0x81acc4. Naked C with
 * named assembler instructions retains the nonstandard all-GPR/EFLAGS and
 * XMM-result contract, exact memory access widths and read-before-write order.
 * See recovery/SIMD.md for the recovered lane arithmetic and CPU requirements.
 */
#include "simd.h"

#define ARGS (uint32_t first __attribute__((unused)), \
              uint32_t second __attribute__((unused)), \
              uint32_t third __attribute__((unused)))
#define VECTOR(name, instructions) \
    __attribute__((naked, noinline)) void name ARGS \
    { __asm__(instructions); __builtin_unreachable(); }
#define ENTER \
    "pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %eax\n\tpushl %ebx\n\tpushl %ecx\n\t"
#define POINTERS \
    "movl 8(%ebp),%eax\n\tmovl 12(%ebp),%ebx\n\tmovl 16(%ebp),%ecx\n\t"
#define LEAVE "popl %ecx\n\tpopl %ebx\n\tpopl %eax\n\tleave\n\tret"
#define LOAD_PAIR "movdqu (%eax),%xmm0\n\tmovdqu (%ebx),%xmm1\n\t"

VECTOR(kernel_simd_add_81ab34, ENTER POINTERS LOAD_PAIR
    "paddb %xmm1,%xmm0\n\tmovdqu %xmm0,(%ecx)\n\t" LEAVE)
VECTOR(kernel_simd_subtract_81ab58, ENTER POINTERS LOAD_PAIR
    "psubb %xmm1,%xmm0\n\tmovdqu %xmm0,(%ecx)\n\t" LEAVE)
VECTOR(kernel_simd_multiply_81ab7c, ENTER POINTERS LOAD_PAIR
    "pmovzxbw %xmm0,%xmm2\n\tpmovzxbw %xmm1,%xmm3\n\t"
    "pmullw %xmm3,%xmm2\n\t"
    "psrldq $8,%xmm0\n\tpsrldq $8,%xmm1\n\t"
    "pmovzxbw %xmm0,%xmm4\n\tpmovzxbw %xmm1,%xmm5\n\t"
    "pmullw %xmm5,%xmm4\n\tpackuswb %xmm4,%xmm2\n\t"
    "movdqu %xmm2,(%ecx)\n\t" LEAVE)

/* Replicate by OR-ing shifted DWORD bytes, not by broadcasting one byte.
 * register MOVSD preserves the destination's upper 64 bits, which are
 * observable XMM outputs even where they do not affect the pixel result.
 */
VECTOR(kernel_simd_blend_81abc6, ENTER "pushl %edx\n\t" POINTERS
    "pxor %xmm7,%xmm7\n\tmovl $0x808080,%edx\n\t"
    "movd %edx,%xmm6\n\tpshufd $0,%xmm6,%xmm6\n\t"
    "movsd (%eax),%xmm0\n\tmovsd (%ecx),%xmm1\n\t"
    "movsd %xmm1,%xmm5\n\tpslld $8,%xmm5\n\tpor %xmm5,%xmm1\n\t"
    "pslld $8,%xmm5\n\tpor %xmm5,%xmm1\n\t"
    "punpcklbw %xmm7,%xmm0\n\tpunpcklbw %xmm7,%xmm1\n\t"
    "pmullw %xmm1,%xmm0\n\t"
    "movsd 8(%eax),%xmm1\n\tmovsd 8(%ecx),%xmm2\n\t"
    "movsd %xmm2,%xmm5\n\tpslld $8,%xmm5\n\tpor %xmm5,%xmm2\n\t"
    "pslld $8,%xmm5\n\tpor %xmm5,%xmm2\n\t"
    "punpcklbw %xmm7,%xmm1\n\tpunpcklbw %xmm7,%xmm2\n\t"
    "pmullw %xmm2,%xmm1\n\t"
    "movsd (%ebx),%xmm2\n\tmovsd (%ecx),%xmm5\n\t"
    "movsd %xmm5,%xmm3\n\tpslld $8,%xmm3\n\tpor %xmm3,%xmm5\n\t"
    "pslld $8,%xmm3\n\tpor %xmm3,%xmm5\n\t"
    "movsd %xmm6,%xmm3\n\tpsubb %xmm5,%xmm3\n\t"
    "punpcklbw %xmm7,%xmm2\n\tpunpcklbw %xmm7,%xmm3\n\t"
    "pmullw %xmm3,%xmm2\n\t"
    "movsd 8(%ebx),%xmm3\n\tmovsd 8(%ecx),%xmm5\n\t"
    "movsd %xmm5,%xmm4\n\tpslld $8,%xmm4\n\tpor %xmm4,%xmm5\n\t"
    "pslld $8,%xmm4\n\tpor %xmm4,%xmm5\n\t"
    "movsd %xmm6,%xmm4\n\tpsubb %xmm5,%xmm4\n\t"
    "punpcklbw %xmm7,%xmm3\n\tpunpcklbw %xmm7,%xmm4\n\t"
    "pmullw %xmm4,%xmm3\n\t"
    "paddw %xmm2,%xmm0\n\tpaddw %xmm3,%xmm1\n\t"
    "psrlw $7,%xmm0\n\tpsrlw $7,%xmm1\n\tpackuswb %xmm1,%xmm0\n\t"
    "movdqu %xmm0,(%eax)\n\tpopl %edx\n\t" LEAVE)

#undef ARGS
#undef VECTOR
#undef ENTER
#undef POINTERS
#undef LEAVE
#undef LOAD_PAIR
