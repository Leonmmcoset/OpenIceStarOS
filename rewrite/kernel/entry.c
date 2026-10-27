/* Kernel entry 0x800040..0x800070 and bootstrap 0x800070..0x800104.
 * Keep every call and PIC-mask write in the original order, including STI
 * before PIT/RTC/device initialization. The six address-named callees below
 * remain original code; recovering this orchestration does not recover them. */
#include <stdint.h>
#include "entry.h"

#define ENTRY_BOUNDARY(type, name, address) type name(void);
#include "entry_boundaries.def"
#undef ENTRY_BOUNDARY
void kernel_write_port_byte(uint32_t port, uint32_t value);

__attribute__((noinline)) void kernel_bootstrap(void)
{
    kernel_disable_interrupts();
    kernel_init_global_state();
    kernel_startup_step_81acc4();
    kernel_init_startup_record();
    kernel_init_heap();
    kernel_startup_step_837448();
    kernel_startup_step_82aa0f();
    kernel_init_api_table();
    kernel_init_service_table();
    kernel_init_interrupts();
    kernel_init_pic();
    kernel_enable_interrupts();
    kernel_init_pit();
    kernel_init_rtc();
    kernel_init_keyboard();
    kernel_init_pointer();
    (void)kernel_surfaces_init();
    kernel_write_port_byte(0x21, 0xd8);
    kernel_delay_three_nops();
    kernel_write_port_byte(0xa1, 0xec);
    kernel_scan_acpi();
    kernel_scan_pmid();
}

void kernel_entry(void)
{
    kernel_bootstrap();
    kernel_init_tasks();
    kernel_startup_step_81067a();
    kernel_startup_step_810d4a();
    (void)kernel_cursor_show();
    kernel_startup_step_810f38();
    /* The production callee never returns. Keeping the call/return here
     * also preserves the original orchestration if a test callee returns. */
    kernel_event_loop();
}
