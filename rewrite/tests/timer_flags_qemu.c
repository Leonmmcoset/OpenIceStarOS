/* The real CPL0 flag helpers remain unmodified in all three implementations.
 * Only allocator entrypoints are redirected; PIC IRQs are masked so IF can
 * be observed without requiring the operating system's interrupt handlers. */
#include <stdint.h>
#include "../kernel/timer_objects.h"
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
static uint32_t alloc_calls, free_calls, boundary_failure, allocation, executions;
static uint32_t flags(void)
{
    uint32_t value;
    __asm__ volatile("pushfl; popl %0" : "=r"(value) : : "memory");
    return value;
}
static void interrupt_state(uint32_t enabled)
{
    if (enabled) __asm__ volatile("sti" : : : "memory");
    else __asm__ volatile("cli" : : : "memory");
}
uint32_t kernel_allocate(uint32_t size)
{
    ++alloc_calls;
    if (size != 12 || (flags() & 0x200)) boundary_failure = 1;
    return allocation;
}
void kernel_free_allocation(uint32_t address)
{
    ++free_calls;
    if (address != 0x600100 || (flags() & 0x200)) boundary_failure = 1;
}
static void output(const char *s)
{
    while (*s) __asm__ volatile("outb %0,$0xe9" : : "a"(*s++));
}
static void finish(uint32_t error)
{
    interrupt_state(0);
    if (error) {
        output("FAIL timer CPL0 code: ");
        __asm__ volatile("outb %0,$0xe9" : : "a"((uint8_t)('0'+error/10)));
        __asm__ volatile("outb %0,$0xe9" : : "a"((uint8_t)('0'+error%10)));
        output("\n");
    } else output("PASS: 48 timer CPL0 scenarios, 144 executions; unhooked CLI/PUSHFL/POPFL and IF success/failure paths\n");
    __asm__ volatile("outl %0,$0xf4" : : "a"(error ? 1u : 16u));
    for (;;) __asm__ volatile("hlt");
}
static void redirect(uint32_t address, uint32_t target)
{
    *(volatile uint8_t *)address = 0xe9;
    DWORD(address+1) = target-address-5;
}
static void initialize(void)
{
    uint32_t i;
    interrupt_state(0);
    for (i = 0; i < 16; ++i) DWORD(0x6c0000+i*4) = 0x600100;
    for (i = 0; i < 16; ++i) DWORD(0x600100+i*4) = 0xdeadbeef;
    DWORD(0x1b0bc) = 2; DWORD(0x1b0b8) = 8; DWORD(0x1b0c0) = 0;
    DWORD(0x1b084) = 0xfffffffe;
    alloc_calls = free_calls = boundary_failure = 0;
    allocation = 0x600100;
}
static void create_test(uint32_t implementation, uint32_t enabled, uint32_t scenario, uint32_t delay)
{
    uint32_t address = implementation == 0 ? 0x92aaad : implementation == 2 ? 0x82aaad :
        (uint32_t)kernel_timer_create;
    uint32_t result, expected, failed = scenario >= 2;
    initialize();
    if (scenario == 1) {
        DWORD(0x1b0bc) = DWORD(0x1b0b8) = 4; DWORD(0x1b0c0) = 1;
        DWORD(0x6c0004) = 0;
    } else if (scenario == 2) DWORD(0x1b0b8) = 2;
    else if (scenario == 3) allocation = 0;
    else if (scenario == 4) DWORD(0x1b0bc) = 0xffffffff;
    interrupt_state(enabled);
    result = ((uint32_t (*)(uint32_t, uint32_t))address)(delay, 0x12345678);
    expected = failed ? 0xffffffff : scenario == 1 ? 1 : 2;
    if (((flags() >> 9) & 1) != (failed ? 0 : enabled)) finish(10+implementation);
    interrupt_state(0);
    if (result != expected || boundary_failure || free_calls ||
        alloc_calls != (scenario == 2 || scenario == 4 ? 0u : 1u)) finish(20+implementation);
    if (!failed && (DWORD(0x600100) != 0x12345678 || DWORD(0x600104) != delay-2u ||
        DWORD(0x600108) || DWORD(0x1b0bc) != (scenario == 1 ? 4u : 3u) ||
        DWORD(0x1b0c0))) finish(30+implementation);
    ++executions;
}
static void delete_test(uint32_t implementation, uint32_t enabled, uint32_t value)
{
    uint32_t address = implementation == 0 ? 0x92abaf : implementation == 2 ? 0x82abaf :
        (uint32_t)kernel_timer_delete;
    uint32_t should_free = value == 0x600100;
    initialize(); DWORD(0x6c0004) = value; DWORD(0x1b0c0) = 0xffffffff;
    interrupt_state(enabled);
    ((void (*)(uint32_t))address)(1);
    if (((flags() >> 9) & 1) != enabled) finish(40+implementation);
    interrupt_state(0);
    if (boundary_failure || alloc_calls || free_calls != should_free ||
        DWORD(0x6c0004) != (should_free ? 0 : value) ||
        DWORD(0x1b0c0) != (should_free ? 0 : 0xffffffff)) finish(50+implementation);
    ++executions;
}
void timer_flags_main(void)
{
    uint32_t implementation, enabled, scenario, vector;
    static const uint32_t delays[] = {0, 1, 0x80000000, 0xffffffff};
    static const uint32_t entries[] = {0, 0x600100, 0x80000000, 0xffffffff};
    redirect(0x815464, (uint32_t)kernel_allocate);
    redirect(0x915464, (uint32_t)kernel_allocate);
    redirect(0x815978, (uint32_t)kernel_free_allocation);
    redirect(0x915978, (uint32_t)kernel_free_allocation);
    for (enabled = 0; enabled < 2; ++enabled)
    for (implementation = 0; implementation < 3; ++implementation) {
        for (scenario = 0; scenario < 5; ++scenario)
        for (vector = 0; vector < 4; ++vector)
            create_test(implementation, enabled, scenario, delays[vector]);
        for (vector = 0; vector < 4; ++vector)
            delete_test(implementation, enabled, entries[vector]);
    }
    if (executions != 144) finish(60);
    finish(0);
}
