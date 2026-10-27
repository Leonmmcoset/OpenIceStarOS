#include <stdint.h>

uint32_t kernel_read_flags(void);
void kernel_disable_interrupts(void);
void kernel_write_flags(uint32_t flags);
uint32_t kernel_queue_available(const volatile uint32_t *queue);
uint32_t kernel_queue_read(volatile uint32_t *queue);
uint32_t kernel_decode_pointer(volatile uint32_t *pointer, uint32_t byte);
void kernel_dispatch_event(void);

void kernel_handle_pointer_input(void)
{
    volatile uint32_t *display =
        (volatile uint32_t *)(uintptr_t)*(volatile uint32_t *)0x1b034;
    volatile uint32_t *queue =
        (volatile uint32_t *)(uintptr_t)*(volatile uint32_t *)0x1b044;
    volatile uint32_t *pointer =
        (volatile uint32_t *)(uintptr_t)*(volatile uint32_t *)0x1b048;
    volatile uint32_t *record =
        (volatile uint32_t *)(uintptr_t)*(volatile uint32_t *)0x1b040;
    volatile uint32_t *cursor =
        (volatile uint32_t *)(uintptr_t)*(volatile uint32_t *)0x1b04c;
    uint32_t flags = kernel_read_flags();
    kernel_disable_interrupts();

    while (kernel_queue_available(queue)) {
        uint8_t byte = (uint8_t)kernel_queue_read(queue);
        if (!kernel_decode_pointer(pointer, byte)) continue;
        if (pointer[1] != 0 || pointer[2] != 0) {
            /* DWORD arithmetic wraps before the original signed comparisons. */
            pointer[3] = pointer[3] + pointer[1];
            pointer[4] = pointer[4] + pointer[2];
            if ((int32_t)pointer[3] < 0) pointer[3] = 0;
            if ((int32_t)pointer[3] > (int32_t)display[3]) pointer[3] = display[3];
            if ((int32_t)pointer[4] < 0) pointer[4] = 0;
            if ((int32_t)pointer[4] > (int32_t)display[4]) pointer[4] = display[4];
            *(volatile uint32_t *)0x1b078 = 0;
            record[6] = 5;
            if ((int32_t)*(volatile uint32_t *)0x1b080 > 0) record[6] = 9;
            if ((int32_t)pointer[5] > 0) record[6] = 8;
        } else if (pointer[5] == 0 && (int32_t)*(volatile uint32_t *)0x1b080 > 0) {
            record[6] = 9;
            if ((int32_t)(*(volatile uint32_t *)0x1b084 -
                          *(volatile uint32_t *)0x1b078) <
                (int32_t)*(volatile uint32_t *)0x1b07c) {
                record[6] = 7;
            } else {
                *(volatile uint32_t *)0x1b078 = *(volatile uint32_t *)0x1b084;
            }
        } else if ((int32_t)pointer[5] > 0 && *(volatile uint32_t *)0x1b080 == 0) {
            record[6] = 10;
        }
        *(volatile uint32_t *)0x1b080 = pointer[5];
    }
    if (cursor[0x2c / 4] != 0) kernel_dispatch_event();
    record[6] = 0;
    kernel_write_flags(flags);
}
