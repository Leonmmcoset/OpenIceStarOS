#include "cpuid.h"

uint32_t kernel_cpuid_available(void)
{
    uint32_t result;
    /* Test EFLAGS.ID, then restore the saved flags, including DF and ID.
     * Arithmetic flags are not a C ABI result (the original SUB prologue
     * changes them before saving). Keep the push/pop pairs together. */
    __asm__ volatile (
        "pushfl\n\tpushfl\n\txorl $0x200000,(%%esp)\n\tpopfl\n\t"
        "pushfl\n\tpopl %0\n\txorl (%%esp),%0\n\t"
        "andl $0x200000,%0\n\tshrl $21,%0\n\tpopfl"
        : "=a" (result) : : "cc", "memory");
    return result;
}

/* x86 permits the unaligned output buffers accepted by the original code.
 * Volatile stores retain the original DWORD access order and final byte. */
typedef struct __attribute__((packed, may_alias)) { uint32_t value; } unaligned_word;
#define OUTPUT(offset) (((volatile unaligned_word *)((uint8_t *)output + (offset)))->value)

uint32_t kernel_cpuid_vendor(void *output)
{
    uint32_t a, b, c, d;
    __asm__ volatile ("cpuid" : "=a" (a), "=b" (b), "=c" (c), "=d" (d) : "0" (0));
    OUTPUT(0) = a;
    OUTPUT(4) = b;
    OUTPUT(8) = d;
    OUTPUT(12) = c;
    *(volatile uint8_t *)((uint8_t *)output + 16) = 0;
    return (uint32_t)(uintptr_t)output;
}

uint32_t kernel_cpuid_features(void *output)
{
    uint32_t a, b, c, d;
    __asm__ volatile ("cpuid" : "=a" (a), "=b" (b), "=c" (c), "=d" (d) : "0" (1));
    OUTPUT(0) = a;
    OUTPUT(4) = b;
    OUTPUT(8) = c;
    OUTPUT(12) = d;
    return (uint32_t)(uintptr_t)((uint8_t *)output + 12);
}
