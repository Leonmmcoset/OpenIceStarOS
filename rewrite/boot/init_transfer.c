#include "init_transfer.h"

uint32_t stage_prepare_init(uint32_t *header, uint32_t load_address)
{
    header[2] = 0x38;
    header[5] = 0x40;
    header[6] = 0x38;
    return load_address + header[1] + header[3] * 4096 + header[11] - 1;
}
