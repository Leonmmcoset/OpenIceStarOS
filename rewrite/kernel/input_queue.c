#include <stdint.h>

/* Keyboard queues reject a full-buffer write. Mouse queues instead shift
 * their byte array; keep the two original behaviors distinct. */
uint32_t kernel_input_write(volatile uint32_t *queue, uint32_t value)
{
    if (queue[2] >= queue[3]) return 0;
    ((volatile uint8_t *)queue)[16 + queue[1]] = (uint8_t)value;
    ++queue[1];
    ++queue[2];
    if (queue[1] >= queue[3]) queue[1] = 0;
    return 1;
}

uint32_t kernel_input_read(volatile uint32_t *queue)
{
    uint8_t byte = ((volatile uint8_t *)queue)[16 + queue[0]];
    ++queue[0];
    --queue[2];
    if (queue[0] >= queue[3]) queue[0] = 0;
    return byte;
}

uint32_t kernel_input_available(const volatile uint32_t *queue)
{
    return queue[2] != 0;
}

uint32_t kernel_ascii_upper(uint32_t character)
{
    if ((int32_t)character > 0x60 && (int32_t)character <= 0x7a)
        return character - 0x20;
    return character;
}
