/* Surface initialization/allocation/release, 0x8175c0..0x8178f0.
 * The original failure paths deliberately leave interrupts disabled. Do not
 * repair that behavior or normalize the signed counters and raw returns. */
#include "surface_lifecycle.h"
#include "surface_refresh.h"
#include "memory.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define SLOT(n) WORD(0x500000u + 4u * (n))

uint32_t kernel_surfaces_init(void)
{
    SLOT(0) = WORD(0x1b034u);
    SLOT(1) = WORD(0x1b04cu);
    WORD(0x1b070u) = 0x900000u;
    WORD(0x1b074u) = 0x510000u;
    WORD(0x510000u) = 0;
    WORD(0x1b068u) = 2;
    return 0x1b068u;
}

uint32_t kernel_surface_find_free(void)
{
    uint32_t i;
    if ((int32_t)WORD(0x1b06cu) > 0)
        for (i = 0; (int32_t)i < (int32_t)WORD(0x1b068u); ++i)
            if (!SLOT(i)) return i;
    if ((int32_t)WORD(0x1b068u) < (int32_t)WORD(0x1b064u))
        return WORD(0x1b068u);
    return 0xffffffffu;
}

/* Failure after free returns that callback's EFLAGS, without calling the
 * flags-write service. Capture before compiler-generated stack cleanup. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
__attribute__((used, naked, noinline))
static uint64_t surface_create_free_failure(uint32_t descriptor)
{
    __asm__("pushl 4(%esp);call kernel_free_allocation;pushfl;popl %edx;"
        "movl $-1,%eax;leal 4(%esp),%esp;ret");
    __builtin_unreachable();
}
#pragma GCC diagnostic pop

__attribute__((used, noinline, optimize("no-optimize-sibling-calls")))
static uint64_t surface_create_body(volatile uint32_t *a)
{
    uint32_t flags = kernel_read_flags(), descriptor, index, owner, area, depth, pixels;
    kernel_disable_interrupts();
    descriptor = kernel_allocate_for_task(56, WORD(0x1b0d8u));
    /* The search executes even when descriptor allocation failed. */
    index = kernel_surface_find_free();
    if (!descriptor) {
        uint32_t failure_flags;
        __asm__ volatile("cmpl $0,%1;pushfl;popl %0" : "=r"(failure_flags)
                         : "r"(descriptor) : "cc", "memory");
        return ((uint64_t)failure_flags << 32) | 0xffffffffu;
    }
    if ((int32_t)index < 0) return surface_create_free_failure(descriptor);
    WORD(descriptor + 4u) = a[0];
    WORD(descriptor + 8u) = a[1];
    WORD(descriptor + 12u) = a[2];
    WORD(descriptor + 16u) = a[3];
    WORD(descriptor + 20u) = a[2];
    WORD(descriptor + 24u) = a[3];
    WORD(descriptor + 28u) = 100;
    WORD(descriptor + 32u) = 100;
    WORD(descriptor + 36u) = WORD(0x1a816u);
    WORD(descriptor + 40u) = 0xffffffffu;
    WORD(descriptor + 44u) = 0;
    WORD(descriptor + 48u) = 0;
    WORD(descriptor + 52u) = 0;
    owner = WORD(0x1b0d8u);
    area = a[2] * a[3];
    depth = WORD(descriptor + 36u);
    pixels = kernel_allocate_for_task(area * (uint32_t)((int32_t)depth / 8), owner);
    WORD(descriptor) = pixels;
    if (!WORD(descriptor)) return surface_create_free_failure(descriptor);
    SLOT(index) = descriptor;
    if ((int32_t)WORD(0x1b068u) > (int32_t)index) --WORD(0x1b06cu);
    else ++WORD(0x1b068u);
    return ((uint64_t)flags << 32) | index;
}

__attribute__((used, noinline, optimize("no-optimize-sibling-calls")))
static uint32_t surface_delete_body(volatile uint32_t *a)
{
    uint32_t flags = kernel_read_flags(), descriptor;
    kernel_disable_interrupts();
    if (SLOT(a[0])) {
        descriptor = SLOT(a[0]);
        if (WORD(descriptor + 44u)) kernel_surface_order_remove(WORD(descriptor + 44u));
        if (WORD(descriptor)) kernel_free_allocation(WORD(descriptor));
        /* Reload the live index/table after callbacks; the descriptor used
         * above remains cached even when removal replaces this slot. */
        kernel_free_allocation(SLOT(a[0]));
        SLOT(a[0]) = 0;
        ++WORD(0x1b06cu);
    }
    return flags;
}

/* Expose original caller storage to aliasing descriptor stores/callbacks.
 * LEA/LEAVE/RET preserve the flags restored by the final hardware call. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
__attribute__((naked, noinline))
uint32_t kernel_surface_create(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    __asm__("pushl %ebp;movl %esp,%ebp;subl $24,%esp;"
        "leal 8(%ebp),%eax;movl %eax,(%esp);call surface_create_body;"
        "cmpl $-1,%eax;jne 1f;pushl %edx;popfl;jmp 2f;"
        "1:movl %eax,-4(%ebp);movl %edx,(%esp);"
        "call kernel_write_flags;movl -4(%ebp),%eax;2:leave;ret");
    __builtin_unreachable();
}

__attribute__((naked, noinline))
uint32_t kernel_surface_delete(uint32_t layer)
{
    __asm__("leal 4(%esp),%eax;pushl %eax;call surface_delete_body;"
        "leal 4(%esp),%esp;pushl %eax;call kernel_write_flags;"
        "leal 4(%esp),%esp;ret");
    __builtin_unreachable();
}
#pragma GCC diagnostic pop
