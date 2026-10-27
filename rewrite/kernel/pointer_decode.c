#include <stdint.h>

uint32_t kernel_decode_pointer(volatile uint32_t *pointer, uint32_t value)
{
    volatile uint8_t *packet = (volatile uint8_t *)pointer;
    uint8_t byte = (uint8_t)value;
    if (packet[3] == 0) {
        if (byte == 0xfa) packet[3] = 1;
    } else if (packet[3] == 1) {
        if ((byte & 0xc8) == 8) {
            packet[0] = byte;
            packet[3] = 2;
        }
    } else if (packet[3] == 2) {
        packet[1] = byte;
        packet[3] = 3;
    } else if (packet[3] == 3) {
        packet[2] = byte;
        packet[3] = 1;
        pointer[5] = packet[0] & 7;
        pointer[1] = packet[1];
        pointer[2] = packet[2];
        if (packet[0] & 0x10) pointer[1] |= 0xffffff00;
        if (packet[0] & 0x20) pointer[2] |= 0xffffff00;
        pointer[2] = 0u - pointer[2];
        return 1;
    }
    return 0;
}
