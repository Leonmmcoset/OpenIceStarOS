#include "syscall_services.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))

/* The original object pointer table is fixed at 0x6a0000. Indices, pointer
 * arithmetic and the setter's EAX result wrap at 32 bits without validation.
 * Field names remain numeric until their full object layout is recovered. */
uint32_t kernel_syscall_target_81041a(uint32_t index,uint32_t event)
{
    uint32_t object=WORD(0x6a0000+4*index);
    WORD(object+0x55f)=event;
    return event;
}
uint32_t kernel_syscall_target_810448(uint32_t index)
{
    uint32_t object=WORD(0x6a0000+4*index);
    return WORD(object+12);
}
uint32_t kernel_syscall_target_810478(uint32_t index)
{
    uint32_t object=WORD(0x6a0000+4*index);
    return WORD(object+4);
}

/* The low TSC word is sampled once. Keep all three logical xorshift stages
 * and the original modulo formula, including a CPU divide fault when the
 * wrapped span is zero; do not normalize reversed or full-width bounds. */
uint32_t kernel_syscall_target_82a9bb(uint32_t lower,uint32_t upper)
{
    uint32_t n=kernel_timestamp_low();
    n^=n>>6;n^=n<<12;n^=n>>14;
    return n%(upper-lower+1)+lower;
}
