/* Firmware discovery, 0x815106..0x815320. These are the original algorithms,
 * including the seven-byte RSD signature and linear FACP candidate walk.
 * They deliberately do not implement a corrected ACPI table parser. */
#include <stdint.h>
#include "firmware_scan.h"

void kernel_write_port_byte(uint32_t port, uint32_t value);
uint32_t kernel_read_port_word(uint32_t port);
void kernel_write_port_word(uint32_t port, uint32_t value);
void kernel_delay_three_nops(void);

#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))

void kernel_scan_acpi(void)
{
    uint32_t address;
    for (address = 0xe0000u; address <= 0xffffeu; ++address) {
        uint32_t root, length, candidate;
        int32_t count, i;
        if (BYTE(address) != 'R' || BYTE(address + 1u) != 'S' ||
            BYTE(address + 2u) != 'D' || BYTE(address + 3u) != ' ' ||
            BYTE(address + 4u) != 'P' || BYTE(address + 5u) != 'T' ||
            BYTE(address + 6u) != 'R') continue;
        root = WORD(address + 16u);
        length = WORD(root + 4u);
        (void)WORD(root + 36u); /* Original performs this unused read as well. */
        candidate = WORD(root + 36u);
        count = (int32_t)(length - 36u) / 4;
        for (i = 0; i < count; ++i, candidate += 4u) {
            uint32_t port_a, port_b, command, control;
            if (BYTE(candidate) != 'F' || BYTE(candidate + 1u) != 'A' ||
                BYTE(candidate + 2u) != 'C' || BYTE(candidate + 3u) != 'P') continue;
            port_a = WORD(candidate + 56u);
            port_b = WORD(candidate + 64u);
            control = (uint32_t)(int32_t)(int8_t)BYTE(candidate + 52u);
            command = WORD(candidate + 48u);
            (void)*(volatile int16_t *)(uintptr_t)(candidate + 46u);
            kernel_write_port_byte(command, control);
            (void)kernel_read_port_word(port_b);
            (void)kernel_read_port_word(port_a);
            kernel_write_port_word(port_a, 16);
            kernel_delay_three_nops();
            kernel_write_port_word(port_a + 2u, 16);
            WORD(0x1b0dcu) = port_b;
            break; /* The outer bytewise signature search continues. */
        }
    }
}

void kernel_scan_pmid(void)
{
    uint32_t address;
    for (address = 0; address <= 0xffffeu; ++address) {
        if (BYTE(address) == 'P' && BYTE(address + 1u) == 'M' &&
            BYTE(address + 2u) == 'I' && BYTE(address + 3u) == 'D') {
            WORD(0x1b174u) = address;
            return;
        }
    }
}
