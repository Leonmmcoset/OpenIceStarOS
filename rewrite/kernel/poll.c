#include <stdint.h>

void kernel_dispatch_slot1(void);
void kernel_dispatch_slot2(void);
void kernel_dispatch_slot3(void);
void kernel_dispatch_api_request(void);
void kernel_handle_pointer_input(void);
uint32_t kernel_input_available(const volatile uint32_t *queue);
uint32_t kernel_queue_available(const volatile uint32_t *queue);

void kernel_poll_events(void)
{
    volatile uint32_t *record =
        (volatile uint32_t *)(uintptr_t)*(volatile uint32_t *)0x1b040;
    if (record[0]) {
        kernel_dispatch_slot1();
        record[0] = 0;
    }
    if (record[1]) {
        kernel_dispatch_slot2();
        record[1] = 0;
    }
    if (kernel_input_available((const volatile uint32_t *)(uintptr_t)
                               *(volatile uint32_t *)0x1b038)) {
        kernel_dispatch_slot3();
        record[2] = 0;
    }
    if (kernel_queue_available((const volatile uint32_t *)(uintptr_t)
                               *(volatile uint32_t *)0x1b044)) {
        kernel_handle_pointer_input();
        record[3] = 0;
    }
    if (record[4]) {
        kernel_dispatch_api_request();
        record[4] = 0;
    }
}

__attribute__((noreturn)) void kernel_event_loop(void)
{
    for (;;) {
        __asm__ volatile ("hlt" : : : "memory");
        kernel_poll_events();
    }
}
