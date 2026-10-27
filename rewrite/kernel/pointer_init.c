#include <stdint.h>

uint32_t kernel_read_port_byte(uint32_t port);
void kernel_write_port_byte(uint32_t port, uint32_t byte);

__attribute__((noinline)) void kernel_wait_ps2_ready(void)
{
    while (kernel_read_port_byte(0x64) & 2) { }
}

void kernel_init_pointer(void)
{
    volatile uint32_t *display =
        (volatile uint32_t *)(uintptr_t)*(volatile uint32_t *)0x1b034;
    volatile uint32_t *cursor =
        (volatile uint32_t *)(uintptr_t)*(volatile uint32_t *)0x1b04c;
    volatile uint32_t *queue =
        (volatile uint32_t *)(uintptr_t)*(volatile uint32_t *)0x1b044;
    volatile uint32_t *pointer =
        (volatile uint32_t *)(uintptr_t)*(volatile uint32_t *)0x1b048;
    volatile uint8_t *packet = (volatile uint8_t *)pointer;
    cursor[0] = *(volatile uint32_t *)0x1b03c;
    cursor[1] = (uint32_t)((int32_t)display[3] / 2 - 12);
    cursor[2] = (uint32_t)((int32_t)display[4] / 2 - 12);
    cursor[3] = 24;
    cursor[4] = 24;
    cursor[5] = 24;
    cursor[6] = 24;
    cursor[9] = 8;
    cursor[10] = 255;
    cursor[11] = 0;
    cursor[12] = 0;
    cursor[13] = 0;
    queue[0] = 0;
    queue[1] = 0;
    queue[2] = 0;
    queue[3] = 2048;
    packet[0] = 0;
    packet[1] = 0;
    packet[2] = 0;
    packet[3] = 0;
    pointer[1] = 0;
    pointer[2] = 0;
    pointer[3] = (uint32_t)((int32_t)display[3] / 2 - 12);
    pointer[4] = (uint32_t)((int32_t)display[4] / 2 - 12);
    pointer[5] = 0;
    *(volatile uint32_t *)0x1b078 = 0;
    *(volatile uint32_t *)0x1b07c = 50;
    kernel_wait_ps2_ready();
    kernel_write_port_byte(0x64, 0xa8);
    kernel_wait_ps2_ready();
    kernel_write_port_byte(0x64, 0xd4);
    kernel_wait_ps2_ready();
    kernel_write_port_byte(0x60, 0xf4);
    kernel_wait_ps2_ready();
    kernel_write_port_byte(0x64, 0xd4);
    kernel_wait_ps2_ready();
    kernel_write_port_byte(0x60, 0xf4);
}
