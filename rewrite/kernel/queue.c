#include <stdint.h>

/* The full-queue branch shifts the physical byte array, irrespective of the
 * ring indices. This unusual overflow behavior is part of the original ABI. */
uint32_t kernel_queue_write(volatile uint32_t *queue, uint32_t value)
{
    volatile uint8_t *bytes = (volatile uint8_t *)queue + 16;
    uint8_t byte = (uint8_t)value;
    if (queue[2] < queue[3]) {
        bytes[queue[1]] = byte;
        ++queue[1];
        ++queue[2];
        if (queue[1] >= queue[3]) queue[1] = 0;
    } else {
        uint32_t i;
        for (i = 0; i < queue[3] - 1; ++i) bytes[i] = bytes[i + 1];
        bytes[queue[3] - 1] = byte;
    }
    return 1;
}

/* The caller owns the nonempty check, including the original count underflow
 * if this function is deliberately called with an empty queue. */
uint32_t kernel_queue_read(volatile uint32_t *queue)
{
    uint8_t byte = ((volatile uint8_t *)queue)[16 + queue[0]];
    ++queue[0];
    --queue[2];
    if (queue[0] >= queue[3]) queue[0] = 0;
    return byte;
}

uint32_t kernel_queue_available(const volatile uint32_t *queue)
{
    return queue[2] != 0;
}
