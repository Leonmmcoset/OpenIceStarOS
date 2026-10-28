/* X executable allocation/loading and launch at 0x8020a6..0x80252a.
 * Keep the two original header kinds, unchecked/live relocation accesses,
 * allocation/zero/read ordering, task selectors and duplicate-load dialogs.
 * This is the original loader contract, not a hardened X format parser. */
#include "x_loader.h"
#include "memory.h"
#include "memory_ops.h"
#include "tasks.h"
#include "cursor.h"

#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define HEADER 0x18a00u

int32_t kernel_x_find_relative_825c8e(uint32_t name);
void kernel_x_header_relative_827826(uint32_t index, uint32_t destination);
void kernel_x_read_relative_826ade(uint32_t index, uint32_t destination);
int32_t kernel_x_find_path_8263c0(uint32_t name, uint32_t record);
void kernel_x_header_path_8278f4(uint32_t record, uint32_t destination);
void kernel_x_read_path_82760c(uint32_t record, uint32_t destination);
uint32_t kernel_x_dialog_811a42(uint32_t text, uint32_t title, uint32_t style);
void kernel_x_show_dialog_8082c6(uint32_t window);

uint32_t kernel_x_load_relative_8020a6(uint32_t name)
{
    int32_t index = kernel_x_find_relative_825c8e(name);
    uint32_t result = 0;
    if (index >= 0) {
        kernel_x_header_relative_827826((uint32_t)index, HEADER);
        if (DWORD(HEADER) == 0x0au) {
            uint32_t size = DWORD(HEADER + 4u) + DWORD(HEADER + 44u) +
                            (DWORD(HEADER + 12u) << 12);
            result = kernel_allocate(size);
            /* The original zero boundary is called even when allocation fails. */
            kernel_fill_bytes(result, 0, size);
            if (result) kernel_x_read_relative_826ade((uint32_t)index, result);
        }
    }
    return result;
}

uint32_t kernel_x_load_path_80214c(uint32_t name)
{
    uint8_t record[40];
    uint32_t result = 0;
    if (kernel_x_find_path_8263c0(name, (uint32_t)(uintptr_t)record) > 0) {
        kernel_x_header_path_8278f4((uint32_t)(uintptr_t)record, HEADER);
        if (DWORD(HEADER) == 0x0au) {
            uint32_t size = DWORD(HEADER + 4u) + DWORD(HEADER + 44u) +
                            (DWORD(HEADER + 12u) << 12);
            result = kernel_allocate(size);
            kernel_fill_bytes(result, 0, size);
            if (result) kernel_x_read_path_82760c((uint32_t)(uintptr_t)record, result);
        } else if (DWORD(HEADER) == 0x50u) {
            uint32_t size = DWORD(HEADER + 4u) + DWORD(HEADER + 44u) +
                            (DWORD(HEADER + 12u) << 12);
            result = kernel_allocate_aligned(size + 0x1000u, 0, 0x1000u);
            kernel_fill_bytes(result, 0, size + 0x1000u);
            if (result) kernel_x_read_path_82760c((uint32_t)(uintptr_t)record, result);
        }
    }
    return result;
}

uint32_t kernel_startup_launch_image(uint32_t image, uint32_t name)
{
    (void)kernel_cursor_select(1);
    uint32_t kind;
    if (DWORD(image) == 0x0au) kind = 0x0au;
    else if (DWORD(image) == 0x50u) kind = 0x50u;
    else goto finished;

    if (DWORD(image + 28u) == 0) {
        uint32_t size = DWORD(image + 4u) + DWORD(image + 44u) +
                        (DWORD(image + 12u) << 12);
        uint32_t index = 0;
        while (index < DWORD(image + 32u)) {
            uint32_t target = image + DWORD(image + 36u) + index * 4u;
            target = image + DWORD(target);
            DWORD(target) = DWORD(target) + image;
            ++index;
        }
        DWORD(image + 28u) = image;
        uint32_t bytes = ((DWORD(image + 12u) << 10) + DWORD(image + 32u)) * 4u;
        bytes += DWORD(image + 44u);
        kernel_fill_bytes(image + DWORD(image + 36u), 0, bytes);
        if (kind == 0x0au)
            (void)kernel_create_task(image + DWORD(image + 16u), image + size,
                                     0x40, 0x38, 8, image, name);
        else
            (void)kernel_create_application_task(image + DWORD(image + 16u), image + size,
                                                 0x53, 0x4b, 0x50, image, name);
    } else {
        uint32_t window = kernel_x_dialog_811a42(0x83cca0u,
                             kind == 0x0au ? 0x83cc8fu : 0x83cca5u, 1);
        kernel_x_show_dialog_8082c6(window);
    }
finished:
    return kernel_cursor_select(0);
}
