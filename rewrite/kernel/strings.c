/* The original string API at 0x82a32b..0x82a531 is not libc-compatible.
 * Retain its pointer returns, byte tests, signed bounds and null handling. */
#include "helpers.h"

#define BYTE(address) (*(volatile uint8_t *)(uintptr_t)(address))

uint32_t kernel_copy_string(uint32_t destination, uint32_t source)
{
    uint8_t byte;
    if (!destination || !source) return 0;
    do {
        byte = BYTE(source++);
        BYTE(destination++) = byte;
    } while (byte);
    return destination; /* One byte beyond the copied terminator. */
}

uint32_t kernel_copy_text_prefix(uint32_t destination, uint32_t source, int32_t maximum)
{
    uint32_t index = 0, high_bytes = 0;
    if (!destination || !source) return 0;
    for (; (int32_t)index < maximum; ++index) {
        BYTE(destination + index) = BYTE(source + index);
        if (!BYTE(destination + index)) return destination;
        if (BYTE(destination + index) > 0x9fu) ++high_bytes;
    }
    /* Exactly one high byte removes the final copied byte. This is the
       original rule; do not replace it with a different encoding policy. */
    if (high_bytes == 1) BYTE(destination + index - 1u) = 0;
    else BYTE(destination + index) = 0;
    return destination;
}

uint32_t kernel_copy_string_limit(uint32_t destination, uint32_t source, uint32_t count)
{
    if (!destination || !source) return 0;
    while (count-- != 0) {
        BYTE(destination) = BYTE(source);
        if (!BYTE(destination)) return destination;
        ++destination;
        ++source;
    }
    BYTE(destination) = 0;
    return destination; /* Terminator address, unlike kernel_copy_string. */
}

uint32_t kernel_string_length(uint32_t source)
{
    uint32_t index = 0;
    while (BYTE(source + index)) ++index;
    return index;
}

uint32_t kernel_strings_equal(uint32_t first, uint32_t second)
{
    for (;;) {
        uint8_t right = BYTE(second);
        uint8_t left = BYTE(first);
        if (right != left) return 0;
        if (!BYTE(second)) return 1;
        ++second;
        ++first;
    }
}

uint32_t kernel_append_character(uint32_t destination, uint32_t character, uint32_t maximum)
{
    uint32_t length = kernel_string_length(destination);
    if ((int32_t)(maximum - 1u) > (int32_t)length) {
        BYTE(destination + length) = (uint8_t)character;
        BYTE(destination + length + 1u) = 0;
    }
    return destination;
}

void kernel_append_string(uint32_t destination, uint32_t source)
{
    while (BYTE(destination)) ++destination;
    while (BYTE(source)) {
        BYTE(destination) = BYTE(source);
        ++destination;
        ++source;
    }
    BYTE(destination) = 0;
}

#undef BYTE
