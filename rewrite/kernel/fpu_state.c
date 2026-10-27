#include "fpu.h"

/* Preserve each hardware leaf's exact stack ABI and instruction encoding.
 * In particular, the original XSAVE/XRSTOR leaves use EAX both for the
 * memory address and the low feature mask, rather than loading a new mask. */
#define STATE_LEAF(name, instruction) \
    __attribute__((naked, noinline)) void name(uint32_t address __attribute__((unused))) \
    { __asm__("movl 4(%esp),%eax\n\t" instruction " (%eax)\n\tret"); __builtin_unreachable(); }
STATE_LEAF(kernel_fpu_save_legacy, "fnsave")
STATE_LEAF(kernel_fpu_restore_legacy, "frstor")
STATE_LEAF(kernel_fpu_save_environment, "fstenv")
STATE_LEAF(kernel_fpu_load_environment, "fldenv")
STATE_LEAF(kernel_fpu_save_extended, "fxsave")
STATE_LEAF(kernel_fpu_restore_extended, "fxrstor")
STATE_LEAF(kernel_fpu_save_xstate, "xsave")
STATE_LEAF(kernel_fpu_restore_xstate, "xrstor")
#undef STATE_LEAF
