#ifndef OPENICESTAR_KERNEL_INTERRUPT_CALLBACKS_H
#define OPENICESTAR_KERNEL_INTERRUPT_CALLBACKS_H
#include <stdint.h>
void kernel_rtc_callback(void);
void kernel_keyboard_callback(void);
void kernel_audio_callback(void);
void kernel_unused_irq_callback(void);
void kernel_mouse_callback(void);
void kernel_read_cmos_clock(void);

/* Existing kernel boundaries used by the recovered callbacks. */
uint32_t kernel_read_port_byte(uint32_t port);
void kernel_write_port_byte(uint32_t port, uint32_t value);
uint32_t kernel_input_write(volatile uint32_t *queue, uint32_t value);
uint32_t kernel_queue_write(volatile uint32_t *queue, uint32_t value);
#endif
