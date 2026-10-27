#include "pci.h"

void pci_scan(volatile uint32_t *table, pci_read_class_fn read_class,
              void *context)
{
    uint32_t bus, device, function, count = 0;

    table[0] = 0;
    for (bus = 0; bus < 256; ++bus) {
        for (device = 0; device < 32; ++device) {
            for (function = 0; function < 8; ++function) {
                uint32_t address = 0x80000000u | bus << 16 |
                                   device << 11 | function << 8;
                uint32_t value;

                table[count * 2 + 1] = address;
                value = read_class(address, context);
                if (value != 0 && value != 0xffffffffu) {
                    table[0] = ++count;
                    table[count * 2] = value;
                }
            }
        }
    }
}

#ifndef PCI_HOST_TEST
static uint32_t read_class(uint32_t address, void *context)
{
    uint32_t value;
    (void)context;
    __asm__ volatile ("outl %0, %1" : : "a" (address | 8), "Nd" (0xcf8));
    __asm__ volatile ("inl %1, %0" : "=a" (value) : "Nd" (0xcfc));
    return value;
}

void stage_scan_pci(void)
{
    pci_scan((volatile uint32_t *)0x1800, read_class, 0);
    *(volatile uint32_t *)0x4a = 256;
    *(volatile uint32_t *)0x4e = 0;
    *(volatile uint32_t *)0x52 = 0;
}
#endif
