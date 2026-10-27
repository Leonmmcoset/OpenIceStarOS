#include "entry.h"

void kernel_init_startup_record(void);
void kernel_init_global_state(void);

int init_load_file(const char *name, uint8_t *destination)
{
    int directory_index = init_find_root_directory("ICESTAR");
    int file_index;

    if (directory_index < 0 || init_read_root_directory(directory_index, destination) <= 0)
        return 0;
    file_index = init_find_file(destination, name);
    if (file_index < 0) return 0;
    return init_read_file(destination + (uint32_t)file_index * 32, destination) > 0;
}

__attribute__((section(".text.init_entry"))) void init_entry(void)
{
    uint8_t *kernel = (uint8_t *)0x800000;
    int data_loaded, kernel_loaded;

    *(volatile uint32_t *)0x1b108 = 0;
    data_loaded = init_load_file("DAT.BIN", (uint8_t *)0x1b000);
    kernel_loaded = init_load_file("ICESTAR.X", kernel);
    if (data_loaded > 0 && kernel_loaded > 0) {
        /* Redirect the original global-state initializer at 0x80007b. */
        *(uint32_t *)(kernel + 0x7c) =
            (uint32_t)(uintptr_t)kernel_init_global_state - 0x800080;
        /* Redirect the original call at 0x800085 to the recovered routine. */
        *(uint32_t *)(kernel + 0x86) =
            (uint32_t)(uintptr_t)kernel_init_startup_record - 0x80008a;
        uint32_t address = *(uint32_t *)(kernel + 0x10);
        ((void (*)(void))(uintptr_t)address)();
    }
    for (;;) { }
}
