#include "interrupt_callbacks.h"
#define WORD(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define POINTER(address) ((volatile uint32_t *)(uintptr_t)WORD(address))

/* 0x813a8c..0x813aa6. The event pointer is read AFTER clock sampling. */
void kernel_rtc_callback(void)
{
    kernel_read_cmos_clock();
    POINTER(0x1b040)[1] = 1;
}

/* 0x813aa6..0x813ae6. Capture both pointers before the first I/O call;
 * a queue rejection still sets the pending-event flag. */
void kernel_keyboard_callback(void)
{
    volatile uint32_t *queue = POINTER(0x1b038);
    volatile uint32_t *events = POINTER(0x1b040);
    uint32_t byte = (uint8_t)kernel_read_port_byte(0x60);
    kernel_input_write(queue, byte);
    events[2] = 1;
}

/* 0x813ae6..0x813b2a. Retain the DMA mask/DSP acknowledgement order. */
void kernel_audio_callback(void)
{
    kernel_write_port_byte(0x0a, 5);
    kernel_read_port_byte(0x22e);
    kernel_read_port_byte(0x22f);
    WORD(0x1b17c) = 1;
}

/* 0x813b2a..0x813b30. Shared by IRQ9, IRQ10 and IRQ11. */
void kernel_unused_irq_callback(void)
{
}

/* 0x813b30..0x813b70. This uses the mouse queue's shifting-full policy,
 * unlike the keyboard queue's rejection policy. */
void kernel_mouse_callback(void)
{
    volatile uint32_t *queue = POINTER(0x1b044);
    volatile uint32_t *events = POINTER(0x1b040);
    uint32_t byte = (uint8_t)kernel_read_port_byte(0x60);
    kernel_queue_write(queue, byte);
    events[3] = 1;
}
