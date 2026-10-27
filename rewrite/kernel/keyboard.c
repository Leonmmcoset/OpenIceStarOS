#include <stdint.h>

uint32_t kernel_physical_read_byte(uint32_t address);
uint32_t kernel_physical_read_dword(uint32_t address);
void kernel_physical_write_dword(uint32_t address, uint32_t value);
uint32_t kernel_ascii_upper(uint32_t character);
uint32_t kernel_input_read(volatile uint32_t *queue);
uint32_t kernel_input_available(const volatile uint32_t *queue);
void kernel_set_keyboard_leds(uint32_t value);
void kernel_format_text(uint32_t arg0, uint32_t arg1, uint32_t arg2,
                        uint32_t arg3, uint32_t arg4, const char *format, ...);

uint32_t kernel_translate_scancode(uint32_t scancode)
{
    uint32_t count = kernel_physical_read_dword(0x1b018) ? 0x42 : 0x34;
    uint32_t table, i;
    if (scancode != 0xe0 && kernel_physical_read_dword(0x1b02c)) {
        kernel_physical_write_dword(0x1b02c, 0);
        return 0;
    }
    if (!kernel_physical_read_dword(0x1b00c) &&
        !kernel_physical_read_dword(0x1b010))
        table = kernel_physical_read_dword(0x1b024);
    else
        table = kernel_physical_read_dword(0x1b028);
    for (i = 0; i < count; ++i, table += 2) {
        if (kernel_physical_read_byte(table) == scancode) {
            uint32_t character = kernel_physical_read_byte(table + 1);
            if (kernel_physical_read_dword(0x1b014))
                return kernel_ascii_upper(character);
            return character;
        }
    }
    return 0;
}

void kernel_drain_keyboard(void)
{
    volatile uint32_t *queue =
        (volatile uint32_t *)(uintptr_t)*(volatile uint32_t *)0x1b038;
    while (kernel_input_available(queue)) {
        /* Both original mode branches discard a byte; retain the mode read. */
        (void)*(volatile uint32_t *)0x1b0a0;
        (void)kernel_input_read(queue);
    }
}

uint32_t kernel_update_keyboard_state(uint32_t scancode)
{
    uint8_t leds = (uint8_t)kernel_physical_read_byte(0x1a812);
    if (scancode == 0x2a) kernel_physical_write_dword(0x1b00c, 1);
    if (scancode == 0xaa) kernel_physical_write_dword(0x1b00c, 0);
    if (scancode == 0x36) kernel_physical_write_dword(0x1b010, 1);
    if (scancode == 0xb6) kernel_physical_write_dword(0x1b010, 0);
    if (scancode == 0x45) {
        if (kernel_physical_read_dword(0x1b018)) {
            kernel_physical_write_dword(0x1b018, 0);
            leds &= 0xfd;
        } else {
            kernel_physical_write_dword(0x1b018, 1);
            leds |= 2;
        }
        kernel_set_keyboard_leds((uint32_t)(int32_t)(int8_t)leds);
    }
    if (scancode == 0x3a) {
        if (kernel_physical_read_dword(0x1b014)) {
            kernel_physical_write_dword(0x1b014, 0);
            leds &= 0xfb;
        } else {
            kernel_physical_write_dword(0x1b014, 1);
            leds |= 4;
        }
        kernel_set_keyboard_leds((uint32_t)(int32_t)(int8_t)leds);
    }
    if (scancode == 0x46) {
        if (kernel_physical_read_dword(0x1b01c)) {
            kernel_physical_write_dword(0x1b01c, 0);
            leds &= 0xfe;
        } else {
            kernel_physical_write_dword(0x1b01c, 1);
            leds |= 1;
        }
        kernel_set_keyboard_leds((uint32_t)(int32_t)(int8_t)leds);
    }
    /* Preserve the original nonzero guard, including for E1. */
    if ((scancode == 0xe0 || scancode == 0xe1) &&
        kernel_physical_read_dword(0x1b02c))
        kernel_physical_write_dword(0x1b02c, 1);
    return scancode;
}

uint32_t kernel_read_draw_character(uint32_t arg0, uint32_t arg1, uint32_t arg2,
                                    uint32_t arg3, uint32_t arg4)
{
    volatile uint32_t *queue =
        (volatile uint32_t *)(uintptr_t)*(volatile uint32_t *)0x1b038;
    uint32_t scancode = (uint8_t)kernel_input_read(queue);
    uint32_t character = kernel_translate_scancode(scancode);
    if (character) {
        kernel_format_text(arg0, arg1, arg2, arg3, arg4,
                           (const char *)0x83ceaf, character);
        return 1;
    }
    (void)kernel_update_keyboard_state(scancode);
    return 0;
}
