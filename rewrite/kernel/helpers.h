#ifndef ICESTAR_KERNEL_HELPERS_H
#define ICESTAR_KERNEL_HELPERS_H
#include <stdint.h>

void kernel_set_segment_descriptor(uint32_t address, uint32_t limit,
                                   uint32_t base, uint32_t access);
uint32_t kernel_copy_string(uint32_t destination, uint32_t source);
uint32_t kernel_copy_text_prefix(uint32_t destination, uint32_t source, int32_t maximum);
uint32_t kernel_copy_string_limit(uint32_t destination, uint32_t source, uint32_t count);
uint32_t kernel_string_length(uint32_t source);
uint32_t kernel_strings_equal(uint32_t first, uint32_t second);
uint32_t kernel_append_character(uint32_t destination, uint32_t character, uint32_t maximum);
void kernel_append_string(uint32_t destination, uint32_t source);
#endif
