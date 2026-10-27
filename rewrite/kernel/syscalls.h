#ifndef ICESTAR_KERNEL_SYSCALLS_H
#define ICESTAR_KERNEL_SYSCALLS_H
#include <stdint.h>

/* PUSHAD's stack order, passed directly by the two interrupt entries. EBX
 * points to a record whose first three DWORDs are skipped; EDX is the opcode. */
#define SYSCALL_FRAME uint32_t edi, uint32_t esi, uint32_t ebp, uint32_t saved_esp, \
    uint32_t ebx, uint32_t edx, uint32_t ecx, uint32_t eax
void kernel_dispatch_syscall40(SYSCALL_FRAME);
uint32_t kernel_dispatch_syscall41(SYSCALL_FRAME);

/* Verified call signatures; numeric names avoid inventing service semantics
 * for callees whose implementations have not yet been recovered. */
uint32_t kernel_syscall_target_8082c6(uint32_t a0);
uint32_t kernel_syscall_target_808378(uint32_t a0);
uint32_t kernel_syscall_target_81021c(uint32_t a0);
uint32_t kernel_syscall_target_81017c(uint32_t a0);
uint32_t kernel_syscall_target_81041a(uint32_t a0, uint32_t a1);
uint32_t kernel_syscall_target_807a0a(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3,
    uint32_t a4, uint32_t a5, uint32_t a6, uint32_t a7);
uint32_t kernel_syscall_target_810478(uint32_t a0);
uint32_t kernel_syscall_target_810448(uint32_t a0);
uint32_t kernel_syscall_target_82a9bb(uint32_t a0, uint32_t a1);
void kernel_queue_api_request(uint32_t index, uint32_t argument);
#endif
