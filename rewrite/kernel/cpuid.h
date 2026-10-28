#ifndef OPENICESTAR_KERNEL_CPUID_H
#define OPENICESTAR_KERNEL_CPUID_H
#include <stdint.h>

uint32_t kernel_cpuid_available(void);
/* Leaf 0: EAX, EBX, EDX, ECX, followed by one zero byte (17 bytes).
 * Leaf 1: EAX, EBX, ECX, EDX (16 bytes). The original observable EAX
 * is the output address and output address + 12, respectively. */
uint32_t kernel_cpuid_vendor(void *output);
uint32_t kernel_cpuid_features(void *output);
#endif
