#include "syscall_services.h"
/* Original ABI returns only the low TSC word, preserving EDX and EFLAGS. */
__asm__(".section .text.kernel_timestamp_low,\"ax\",@progbits\n"
    ".global kernel_timestamp_low\n.type kernel_timestamp_low,@function\n"
    "kernel_timestamp_low:\npushl %edx\nrdtsc\npopl %edx\nret\n"
    ".size kernel_timestamp_low,.-kernel_timestamp_low\n.previous\n");
