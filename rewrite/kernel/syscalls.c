#include "syscalls.h"
#include "service_forward.h"
#include "runtime.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(a))

/* Each declaration is a separate volatile load: retain the original descending
 * order and load precisely the selected operation's argument count. Explicit
 * 32-bit address arithmetic also retains the original wrapping offsets. */
#define LOAD_1 uint32_t a0=WORD(ebx+12)
#define LOAD_2 uint32_t a1=WORD(ebx+16); LOAD_1
#define LOAD_3 uint32_t a2=WORD(ebx+20); LOAD_2
#define LOAD_4 uint32_t a3=WORD(ebx+24); LOAD_3
#define LOAD_5 uint32_t a4=WORD(ebx+28); LOAD_4
#define LOAD_6 uint32_t a5=WORD(ebx+32); LOAD_5
#define LOAD_6S uint32_t a5=(uint32_t)(int32_t)(int8_t)WORD(ebx+32); LOAD_5
#define LOAD_7 uint32_t a6=WORD(ebx+36); LOAD_6
#define LOAD_8 uint32_t a7=WORD(ebx+40); LOAD_7
#define ARGS_1 a0
#define ARGS_2 ARGS_1,a1
#define ARGS_3 ARGS_2,a2
#define ARGS_4 ARGS_3,a3
#define ARGS_5 ARGS_4,a4
#define ARGS_6 ARGS_5,a5
#define ARGS_6S ARGS_6
#define ARGS_7 ARGS_6,a6
#define ARGS_8 ARGS_7,a7
#define UNUSED_FRAME (void)edi;(void)esi;(void)ebp;(void)saved_esp;(void)ecx;(void)eax

/* A real outgoing-argument area is required: a tail call could overwrite the
 * caller's saved PUSHAD registers. 0x40 deliberately discards service EAX. */
__attribute__((optimize("no-optimize-sibling-calls,no-jump-tables")))
void kernel_dispatch_syscall40(SYSCALL_FRAME)
{
    UNUSED_FRAME;
    switch(edx){
#define ROUTE(op, target, form) case op: { LOAD_##form; target(ARGS_##form); break; }
    ROUTE(0, kernel_service_00, 4)
    ROUTE(1, kernel_service_01, 6)
    ROUTE(2, kernel_service_02, 6)
    ROUTE(3, kernel_service_03, 6)
    ROUTE(4, kernel_service_04, 5)
    ROUTE(5, kernel_service_05, 5)
    ROUTE(6, kernel_service_06, 6S)
    ROUTE(7, kernel_service_07, 6)
    ROUTE(8, kernel_service_0e, 5)
    ROUTE(9, kernel_service_08, 6)
    ROUTE(10, kernel_service_09, 7)
    ROUTE(11, kernel_service_0c, 6S)
    ROUTE(12, kernel_service_0d, 6)
    ROUTE(13, kernel_syscall_target_8082c6, 1)
    ROUTE(14, kernel_syscall_target_808378, 1)
    ROUTE(15, kernel_syscall_target_81021c, 1)
    ROUTE(16, kernel_syscall_target_81017c, 1)
    case 17: { LOAD_1; kernel_queue_api_request(13,a0); break; }
    ROUTE(18, kernel_syscall_target_81041a, 2)
    case 19: kernel_yield_task(); break;
    default: break;
#undef ROUTE
    }
}

/* 0x41 replaces saved EAX with the selected service's full-width result.
 * Unknown opcodes return zero without reading the parameter record. */
__attribute__((optimize("no-optimize-sibling-calls,no-jump-tables")))
uint32_t kernel_dispatch_syscall41(SYSCALL_FRAME)
{
    UNUSED_FRAME;
    switch(edx){
#define ROUTE(op, target, form) case op: { LOAD_##form; return target(ARGS_##form); }
    ROUTE(0, kernel_syscall_target_807a0a, 8)
    ROUTE(1, kernel_syscall_target_810478, 1)
    ROUTE(2, kernel_syscall_target_810448, 1)
    ROUTE(3, kernel_palette_color, 1)
    ROUTE(4, kernel_syscall_target_82a9bb, 2)
    case 5: return WORD(0x1b084);
    default: return 0;
#undef ROUTE
    }
}
