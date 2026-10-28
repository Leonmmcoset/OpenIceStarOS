#ifndef ICESTAR_SIMD_H
#define ICESTAR_SIMD_H
#include <stdint.h>

/* Three pointers, each to 16 accessible bytes; unaligned/overlapping storage
 * is allowed. These ISA boundaries preserve every GPR and EFLAGS. XMM outputs
 * are part of the original interface, so ordinary compiler intrinsics cannot
 * replace the instruction sequences without an additional ABI adapter.
 */
void kernel_simd_add_81ab34(uint32_t left, uint32_t right, uint32_t output);
void kernel_simd_subtract_81ab58(uint32_t left, uint32_t right, uint32_t output);
/* PACKUSWB interprets each 16-bit product as signed before saturation:
 * products above 32767 yield zero, not 255. Requires SSE4.1, as originally.
 */
void kernel_simd_multiply_81ab7c(uint32_t left, uint32_t right, uint32_t output);
/* Updates four destination pixels using packed weights. This SSE2 service
 * differs from scalar blend4 for alpha bytes and weights outside 0..128.
 */
void kernel_simd_blend_81abc6(uint32_t destination, uint32_t source, uint32_t weights);
#endif
