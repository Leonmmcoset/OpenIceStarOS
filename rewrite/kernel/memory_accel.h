#ifndef OPENICESTAR_KERNEL_MEMORY_ACCEL_H
#define OPENICESTAR_KERNEL_MEMORY_ACCEL_H
#include <stdint.h>

void kernel_copy_dwords_sse(uint32_t destination, uint32_t source, int32_t count);
/* REP leaves write their advanced pointers and zero count back to the caller's
 * argument stack slots. Counts are elements, not bytes, for word/DWORD forms. */
void kernel_rep_copy_bytes(uint32_t destination, uint32_t source, uint32_t count);
void kernel_rep_fill_bytes(uint32_t destination, uint32_t value, uint32_t count);
void kernel_rep_copy_words(uint32_t destination, uint32_t source, uint32_t count);
void kernel_rep_fill_words(uint32_t destination, uint32_t value, uint32_t count);
void kernel_rep_copy_dwords(uint32_t destination, uint32_t source, uint32_t count);
void kernel_rep_fill_dwords(uint32_t destination, uint32_t value, uint32_t count);
/* SSE leaves use do/LOOP semantics: zero is NOT a no-op. */
void kernel_copy_sse16_blocks(uint32_t destination, uint32_t source, uint32_t count);
void kernel_copy_sse64_blocks(uint32_t destination, uint32_t source, uint32_t count);
#endif
