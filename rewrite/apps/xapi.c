#include <stdint.h>

/* Exact original cdecl interrupt boundary: EBX points to the saved EBX,
 * saved EDX, return address, then the first argument at EBX+12. Both scratch
 * registers are restored; the raw EAX result reaches the C caller. */
#define U __attribute__((unused))
#define XAPI(name, vector, slot, parameters) \
    __attribute__((naked, noinline)) uint32_t x_##name parameters \
    { \
        __asm__ volatile ("pushl %edx\n\tpushl %ebx\n\t" \
                          "movl $" #slot ",%edx\n\tmovl %esp,%ebx\n\t" \
                          "int $" #vector "\n\tpopl %ebx\n\tpopl %edx\n\tret"); \
        __builtin_unreachable(); \
    }
#include "xapi.def"
#undef XAPI
#undef U
