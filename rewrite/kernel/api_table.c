#include <stdint.h>

void kernel_init_api_table(void)
{
    volatile uint32_t *table =
        (volatile uint32_t *)(uintptr_t)*(volatile uint32_t *)0x1b0a8;

    table[0] = 0x811e46;
    table[1] = 0x8017f2;
    table[2] = 0x8017f8;
    table[3] = 0x801632;
    table[5] = 0x801c2c;
    table[6] = 0x801942;
    table[7] = 0x8017fe;
    table[8] = 0x801dda;
    table[9] = 0x801b40;
    table[10] = 0x801a34;
    table[11] = 0x81017c;
    table[13] = 0x814e54;
    *(volatile uint32_t *)0x1b0ac = 13;
}
