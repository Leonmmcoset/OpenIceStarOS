#include "fpu.h"
#include "cpuid.h"
#define WORD(address) (*(volatile uint32_t *)(uintptr_t)(address))
uint32_t kernel_read_cr0(void);
void kernel_write_cr0(uint32_t value);

uint32_t kernel_fpu_available(void)
{
    uint32_t features[4], status = 0x55aa, control;
    kernel_cpuid_features(features);
    if (!(features[3] & 1u)) return 0;
    __asm__ volatile ("movl %%cr0,%0" : "=a" (control));
    control &= ~12u;
    __asm__ volatile ("movl %0,%%cr0" : : "a" (control) : "memory");
    __asm__ volatile ("fninit\n\tfnstsw %0" : "+m" (status) : : "memory");
    /* FNSTSW writes only the low word; the original compares a full DWORD. */
    return status == 0;
}

uint32_t kernel_fpu_enable(void)
{
    if (!kernel_fpu_available()) return 0;
    kernel_write_cr0((kernel_read_cr0() | 32u) & ~12u);
    return 1;
}

void kernel_fpu_restore_task(void)
{
    uint32_t table, index, entry;
    kernel_write_cr0(kernel_read_cr0() & ~12u);
    table = WORD(0x1b0c8);
    index = WORD(0x1b0d8);
    entry = table + index * 108u;
    if (index == WORD(entry + 8)) return;
    if (WORD(entry + 32)) {
        /* The original rereads the pointer after testing it. */
        kernel_fpu_restore_extended(WORD(entry + 32));
    } else {
        __asm__ volatile ("fnclex\n\tfninit" : : : "memory");
        WORD(entry + 32) = WORD(0x1b164) + index * 512u;
    }
}

void kernel_fpu_mark_lazy(void)
{
    uint32_t control;
    /* Inline only the privileged register accesses to fit the 24-byte slot. */
    __asm__ volatile ("movl %%cr0,%0" : "=a" (control));
    control |= 12u;
    __asm__ volatile ("movl %0,%%cr0" : : "a" (control) : "memory");
}

__attribute__((optimize("no-optimize-sibling-calls")))
void kernel_fpu_fault(void)
{
    kernel_fpu_restore_task();
}
