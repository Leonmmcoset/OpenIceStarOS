#ifndef BOOT_PCI_H
#define BOOT_PCI_H

#include <stdint.h>

typedef uint32_t (*pci_read_class_fn)(uint32_t address, void *context);

void pci_scan(volatile uint32_t *table, pci_read_class_fn read_class,
              void *context);
void stage_scan_pci(void);

#endif
