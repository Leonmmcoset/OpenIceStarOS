#include "pci.h"
#include <assert.h>
#include <stdint.h>

static uint32_t read_class(uint32_t address, void *context)
{
    uint32_t *calls = context;
    uint32_t bus = *calls / 256;
    uint32_t device = (*calls / 8) % 32;
    uint32_t function = *calls % 8;
    assert(address == (0x80000000u | bus << 16 | device << 11 | function << 8));
    ++*calls;
    if (address == 0x80000000u)
        return 0x06000001;
    if (address == 0x80010a00u)
        return 0x03000002;
    return *calls & 1 ? 0 : 0xffffffffu;
}

int main(void)
{
    uint32_t table[8] = {0};
    uint32_t calls = 0;

    pci_scan(table, read_class, &calls);
    assert(calls == 65536);
    assert(table[0] == 2);
    assert(table[1] == 0x80000000u && table[2] == 0x06000001);
    assert(table[3] == 0x80010a00u && table[4] == 0x03000002);
    assert(table[5] == 0x80ffff00u);
    return 0;
}
