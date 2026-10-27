#include <stdint.h>

uint32_t kernel_read_port_byte(uint32_t port);
void kernel_write_port_byte(uint32_t port, uint32_t byte);
uint32_t kernel_physical_read_byte(uint32_t address);
void kernel_physical_write_byte(uint32_t address, uint32_t byte);

__attribute__((noinline)) void kernel_wait_keyboard_ready(void)
{
    while (kernel_read_port_byte(0x64) & 2) { }
}

__attribute__((noinline)) void kernel_discard_keyboard_byte(void)
{
    (void)kernel_read_port_byte(0x60);
}

__attribute__((noinline)) void kernel_set_keyboard_leds(uint32_t value)
{
    /* The original API narrows, then sign-extends the byte for both calls. */
    uint32_t leds = (uint32_t)(int32_t)(int8_t)value;
    kernel_wait_keyboard_ready();
    kernel_write_port_byte(0x60, 0xed);
    kernel_discard_keyboard_byte();
    kernel_wait_keyboard_ready();
    kernel_write_port_byte(0x60, leds);
    kernel_discard_keyboard_byte();
    kernel_wait_keyboard_ready();
    kernel_write_port_byte(0x60, 0xf4);
    kernel_discard_keyboard_byte();
    kernel_physical_write_byte(0x1a812, leds);
}

void kernel_init_keyboard(void)
{
    volatile uint32_t *queue =
        (volatile uint32_t *)(uintptr_t)*(volatile uint32_t *)0x1b038;
    (void)kernel_physical_read_byte(0x1a812);
    queue[0] = 0;
    queue[1] = 0;
    queue[2] = 0;
    queue[3] = 2048;
    kernel_wait_keyboard_ready();
    kernel_write_port_byte(0x64, 0xae);
    kernel_set_keyboard_leds(2);
    kernel_wait_keyboard_ready();
    kernel_write_port_byte(0x64, 0x60);
    kernel_wait_keyboard_ready();
    kernel_write_port_byte(0x60, 0x47);
}
