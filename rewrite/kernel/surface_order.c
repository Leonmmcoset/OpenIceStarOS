/* Ordered visible layers, 0x8178f0..0x817cd0. The list is one-based.
 * Metadata and incoming argument words may alias the list or each other.
 * Keep the original live reloads, unsigned gates and signed move loops. */
#include "surface_order.h"
#include "surface_refresh.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
/* Build with -fwrapv-pointer for the original i386 scaled-address wrap. */
#define SLOT(n) (((volatile uint32_t *)0x500000u)[(uint32_t)(n)])
#define ORDER(n) (((volatile uint32_t *)0x510000u)[(uint32_t)(n)])
#define POSITION(n) WORD(SLOT(n) + 44u)
#define BODY __attribute__((used, noinline, regparm(1), \
    optimize("no-optimize-sibling-calls", "omit-frame-pointer")))

BODY static uint32_t surface_order_remove_body(volatile uint32_t *a)
{
    uint32_t flags = kernel_read_flags(), i;
    if (POSITION(a[0]) && POSITION(a[0]) <= ORDER(0)) {
        for (i = POSITION(a[0]); i < ORDER(0); ++i) {
            ORDER(i) = ORDER(i + 1u);
            POSITION(ORDER(i)) = i;
        }
        --ORDER(0);
        POSITION(a[0]) = 0;
    }
    return flags;
}

BODY static uint32_t surface_order_append_body(volatile uint32_t *a)
{
    uint32_t flags = kernel_read_flags(), address, descriptor, value;
    if (ORDER(0) < WORD(0x1b064u)) {
        address = 0x510004u + 4u * ORDER(0);
        WORD(address) = a[0];
        ++ORDER(0);
        descriptor = SLOT(a[0]);
        value = ORDER(0);
        WORD(descriptor + 44u) = value;
    }
    return flags;
}

BODY static uint32_t surface_order_front_body(volatile uint32_t *a)
{
    uint32_t flags = kernel_read_flags(), i, address, descriptor, value;
    if (POSITION(a[0]) && POSITION(a[0]) <= ORDER(0)) {
        for (i = POSITION(a[0]); i < ORDER(0); ++i) {
            ORDER(i) = ORDER(i + 1u);
            POSITION(ORDER(i)) = i;
        }
        address = 0x510000u + 4u * ORDER(0);
        WORD(address) = a[0];
        descriptor = SLOT(a[0]);
        value = ORDER(0);
        WORD(descriptor + 44u) = value;
    }
    return flags;
}

/* EDX marks the exceptional equal-position return, which bypasses the
 * flags-write callback and exposes the original comparison's EAX/EFLAGS. */
BODY static uint64_t surface_order_move_body(volatile uint32_t *a)
{
    uint32_t flags = kernel_read_flags(), i, address, descriptor, value;
    if (POSITION(a[0]) && (int32_t)a[1] > 0 && a[1] <= ORDER(0)) {
        value = POSITION(a[0]);
        if (value == a[1]) return (UINT64_C(1) << 32) | value;
        if (POSITION(a[0]) < a[1])
            for (i = POSITION(a[0]); (int32_t)i < (int32_t)a[1]; ++i) {
                ORDER(i) = ORDER(i + 1u);
                POSITION(ORDER(i)) = i;
            }
        /* The forward stores can change the caller, table or descriptor;
         * re-read the current position before testing the other direction. */
        if (POSITION(a[0]) > a[1])
            for (i = POSITION(a[0]); (int32_t)i > (int32_t)a[1]; --i) {
                ORDER(i) = ORDER(i - 1u);
                POSITION(ORDER(i)) = i;
            }
        address = 0x510000u + 4u * a[1];
        WORD(address) = a[0];
        descriptor = SLOT(a[0]);
        value = a[1];
        WORD(descriptor + 44u) = value;
    }
    return flags;
}

/* Only ABI boundaries use assembly: expose live incoming argument storage,
 * retain the flags-write callback's raw EAX and preserve EFLAGS through RET. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define ADAPTER(name) \
__attribute__((naked, noinline)) uint32_t kernel_surface_order_##name(uint32_t layer) \
{ \
    __asm__("leal 4(%esp),%eax;call surface_order_" #name "_body;" \
        "pushl %eax;call kernel_write_flags;" \
        "leal 4(%esp),%esp;ret"); \
    __builtin_unreachable(); \
}
ADAPTER(remove)
ADAPTER(append)
ADAPTER(front)

__attribute__((naked, noinline))
uint32_t kernel_surface_order_move(uint32_t layer, uint32_t position)
{
    __asm__("leal 4(%esp),%eax;call surface_order_move_body;"
        "testl %edx,%edx;jne 1f;"
        "pushl %eax;call kernel_write_flags;leal 4(%esp),%esp;ret;"
        "1:cmpl %eax,%eax;ret");
    __builtin_unreachable();
}
#pragma GCC diagnostic pop
