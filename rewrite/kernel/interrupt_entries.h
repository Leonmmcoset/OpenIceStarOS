#ifndef ICESTAR_KERNEL_INTERRUPT_ENTRIES_H
#define ICESTAR_KERNEL_INTERRUPT_ENTRIES_H

/* Interrupt-frame entry points: entered by the CPU and leave with IRET.
 * They are not ordinary C-callable functions. */
#define ENTRY(name, start, end) void kernel_##name(void);
#include "interrupt_entries.def"
#undef ENTRY

#endif
