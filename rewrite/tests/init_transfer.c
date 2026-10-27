#include <assert.h>
#include <stdint.h>
#include "init_transfer.h"

int main(void)
{
    uint32_t header[16] = {10, 26817, 0, 16, 0x70040, 0, 0, 0x70000};
    uint32_t stack = stage_prepare_init(header, 0x70000);
    assert(stack == 0x868c0);
    assert(header[2] == 0x38 && header[5] == 0x40 && header[6] == 0x38);
    assert(header[4] == 0x70040 && header[7] == 0x70000);
    header[11] = 0x100;
    assert(stage_prepare_init(header, 0x70000) == 0x869c0);
    return 0;
}
