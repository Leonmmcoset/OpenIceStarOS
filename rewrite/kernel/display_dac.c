/* VGA DAC writer at 0x81ad6e..0x81ae14. Keep component reads separated by
 * I/O calls, signed comparisons, 32-bit wrapping and the final POPFL boundary. */
#include "display_dac.h"

uint32_t kernel_read_flags(void);
void kernel_disable_interrupts(void);
void kernel_write_flags(uint32_t flags);
void kernel_write_port_byte(uint32_t port, uint32_t value);

__attribute__((used, noinline))
static uint64_t display_dac_body(uint32_t first, uint32_t last, uint32_t rgb)
{
    uint32_t flags = kernel_read_flags(), index;
    kernel_disable_interrupts();
    kernel_write_port_byte(0x3c8, first);
    for (index = first; (int32_t)index <= (int32_t)last; ++index) {
        kernel_write_port_byte(0x3c9, *(volatile uint8_t *)(uintptr_t)rgb >> 2);
        kernel_write_port_byte(0x3c9, *(volatile uint8_t *)(uintptr_t)(rgb + 1u) >> 2);
        kernel_write_port_byte(0x3c9, *(volatile uint8_t *)(uintptr_t)(rgb + 2u) >> 2);
        rgb += 3u;
    }
    return flags | ((uint64_t)rgb << 32);
}

/* The original advances its incoming RGB stack slot and returns directly
 * after restoring EFLAGS. Ordinary C stack cleanup may change those flags.
 * Keep this small ABI boundary in assembly; the DAC algorithm above is C. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
__attribute__((naked, noinline))
void kernel_display_write_palette(uint32_t first, uint32_t last, uint8_t *rgb)
{
    __asm__("pushl %ebp;movl %esp,%ebp;"
        "pushl 16(%ebp);pushl 12(%ebp);pushl 8(%ebp);"
        "call display_dac_body;movl %edx,16(%ebp);"
        "movl %eax,(%esp);call kernel_write_flags;leave;ret");
    __builtin_unreachable();
}
#pragma GCC diagnostic pop
