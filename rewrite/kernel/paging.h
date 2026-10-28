#ifndef OPENICESTAR_KERNEL_PAGING_H
#define OPENICESTAR_KERNEL_PAGING_H
#include <stdint.h>

/* Original 0x837448: build the 4 GiB identity map and enable CR0.PG. */
uint32_t kernel_init_identity_paging(void);

#endif
