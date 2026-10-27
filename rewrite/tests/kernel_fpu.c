/* Execute complete original and recovered routines instruction by instruction.
 * Ptrace models only CPUID, CR0 and FPU hardware instruction boundaries.
 * Four hardware watchpoints also validate captured globals and pointer rereads.
 * The eight state leaves must additionally match the original bytes exactly. */
#include "../kernel/fpu.h"
#include "../kernel/cpuid.h"
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define PRINT(s) syscall3(4, 1, (uint32_t)(uintptr_t)(s), sizeof(s) - 1)
#define USED __attribute__((used))
#define LOW ((volatile uint8_t *)0x1b000)
#define DATA ((volatile uint8_t *)0x600000)
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(a))
static USED uint32_t entry_point, call_flags, output_address;
static uint32_t cases, function_index, initial_cr0, feature, status_word;
static uint32_t task_index, context_mode, mutation, entry_address;
static uint32_t watched_phase, hardware_sites[64], hardware_site_count;
static uint32_t events[32][3], event_count, cr0, reads[4];
static uint32_t expected_events[32][3], expected_count, expected_cr0;
static uint8_t expected_low[0x1000], expected_data[0x4000];
static const uint32_t words[] = {0, 1, 12, 32, 0x8000000d, 0xdeadbeef, 0xffffffff};
static const uint32_t indices[] = {0, 1, 2, 255, 0x7fffffff, 0x80000000, 0xffffffff, 0x40000001};
static const uint32_t statuses[] = {0, 1, 0x55aa, 0x8000, 0xffff};
static const uint32_t entries[] = {
    0x837f5f, 0x837fb5, 0x837ffd, 0x8380c9, 0x8380e1,
    0x83730a, 0x837311, 0x837318, 0x837320,
    0x837327, 0x83732f, 0x837337, 0x83733f
};
static const uint32_t functions[] = {
    (uint32_t)kernel_fpu_available, (uint32_t)kernel_fpu_enable,
    (uint32_t)kernel_fpu_restore_task, (uint32_t)kernel_fpu_mark_lazy,
    (uint32_t)kernel_fpu_fault, (uint32_t)kernel_fpu_save_legacy,
    (uint32_t)kernel_fpu_restore_legacy, (uint32_t)kernel_fpu_save_environment,
    (uint32_t)kernel_fpu_load_environment, (uint32_t)kernel_fpu_save_extended,
    (uint32_t)kernel_fpu_restore_extended, (uint32_t)kernel_fpu_save_xstate,
    (uint32_t)kernel_fpu_restore_xstate
};
struct registers {
    uint32_t ebx, ecx, edx, esi, edi, ebp, eax;
    uint32_t ds, es, fs, gs, original_eax, eip, cs, flags, esp, ss;
};
static struct registers last_registers;
static int syscall3(int number, uint32_t a, uint32_t b, uint32_t c)
{
    int result;
    __asm__ volatile ("int $0x80" : "=a" (result)
                      : "a" (number), "b" (a), "c" (b), "d" (c) : "memory");
    return result;
}
static int trace_call(uint32_t request, uint32_t child, uint32_t address, uint32_t data)
{
    int result;
    __asm__ volatile ("int $0x80" : "=a" (result)
                      : "a" (26), "b" (request), "c" (child), "d" (address), "S" (data) : "memory");
    return result;
}
static void finish(uint32_t code)
{
    syscall3(1, code, 0, 0);
    __builtin_unreachable();
}
static int map_memory(uint32_t address, uint32_t size, uint32_t protection, uint32_t flags)
{
    uint32_t args[] = {address, size, protection, flags, 0xffffffff, 0};
    return (uint32_t)syscall3(90, (uint32_t)(uintptr_t)args, 0, 0) == address;
}
static int load_kernel(const char *path, uint32_t address)
{
    int fd = syscall3(5, (uint32_t)(uintptr_t)path, 0, 0), size;
    if (fd < 0) return 0;
    size = syscall3(3, (uint32_t)fd, address, 0x40000);
    syscall3(6, (uint32_t)fd, 0, 0);
    return size == 250099;
}
static void print_number(uint32_t n)
{
    char digits[10]; uint32_t size = 0;
    do { digits[9 - size++] = (char)('0' + n % 10); n /= 10; } while (n);
    syscall3(4, 1, (uint32_t)(uintptr_t)(digits + 10 - size), size);
}


extern const char after_fpu_call;
__attribute__((naked, noinline)) static void capture_call(void)
{
    __asm__ volatile (
        "movl $0x50fff0,%esp\n\tpushl output_address\n\t"
        "pushl call_flags\n\tpopfl\n\t"
        "movl $0x10293847,%eax\n\tmovl $0x21384957,%ecx\n\t"
        "movl $0x32495867,%edx\n\tmovl $0x435a6977,%ebx\n\t"
        "movl $0x546b7a87,%ebp\n\tmovl $0x657c8b97,%esi\n\t"
        "movl $0x768d9ca7,%edi\n\tint3\n\tcall *entry_point\n\t"
        ".global after_fpu_call\n\tafter_fpu_call:\tint3\n\tcld\n\t"
        "movl $1,%eax\n\txorl %ebx,%ebx\n\tint $0x80");
    __builtin_unreachable();
}
static int wait_child(int child, int *status)
{
    return syscall3(7, (uint32_t)child, (uint32_t)(uintptr_t)status, 0) == child;
}
static void initialize(void)
{
    uint32_t i;
    for (i = 0; i < 0x1000; ++i) LOW[i] = (uint8_t)(i * 37 + cases * 19 + 0x65);
    for (i = 0; i < 0x4000; ++i) DATA[i] = (uint8_t)(i * 13 + cases * 71 + 0x23);
    entry_address = 0x600100 + (cases & 3);
    WORD(0x1b0c8) = entry_address - task_index * 108u;
    WORD(0x1b0d8) = task_index;
    WORD(0x1b164) = 0x603f00u + (cases & 255);
    WORD(entry_address + 8) = context_mode == 0 ? task_index : task_index ^ 0x80000000;
    WORD(entry_address + 32) = context_mode == 2 ? 0x602001 : 0;
    for (i = 0; i < COUNT(reads); ++i) reads[i] = 0;
    event_count = 0; cr0 = initial_cr0;
}
static int event(uint32_t kind, uint32_t a, uint32_t b)
{
    if (event_count == COUNT(events)) return 0;
    events[event_count][0] = kind;
    events[event_count][1] = a;
    events[event_count++][2] = b;
    return 1;
}
static int hardware_site(uint32_t address, int remember)
{
    uint32_t i;
    for (i = 0; i < hardware_site_count; ++i) if (hardware_sites[i] == address) return 1;
    if (!remember || hardware_site_count == COUNT(hardware_sites)) return 0;
    hardware_sites[hardware_site_count++] = address;
    return 1;
}
/* Architectural ModRM register order, unlike Linux's user_regs_struct. */
static uint32_t *reg(struct registers *r, uint32_t number)
{
    switch (number) {
        case 0: return &r->eax; case 1: return &r->ecx;
        case 2: return &r->edx; case 3: return &r->ebx;
        case 4: return &r->esp; case 5: return &r->ebp;
        case 6: return &r->esi; default: return &r->edi;
    }
}
static int execute_case(int original, struct registers *result)
{
    int child, status = 0;
    uint32_t steps = 0, watches[4], i;
    entry_point = entries[function_index] + (original ? 0x100000 : 0);
#ifndef KERNEL_LINKED_TEST
    if (!original) entry_point = functions[function_index];
#endif
    initialize();
    watches[0] = 0x1b0c8; watches[1] = 0x1b0d8;
    watches[2] = entry_address + 32; watches[3] = 0x1b164;
    child = syscall3(2, 0, 0, 0);
    if (child < 0) return 0;
    if (!child) {
        if (trace_call(0, 0, 0, 0) < 0) finish(71);
        syscall3(37, (uint32_t)syscall3(20, 0, 0, 0), 19, 0);
        capture_call(); finish(72);
    }
    if (!wait_child(child, &status) || status != 0x137f) goto failed;
    if (watched_phase) {
        for (i = 0; i < 4; ++i)
            if (trace_call(6, child, 252 + 4 * i, watches[i]) < 0) goto failed;
        /* Linux suppresses data watchpoints during PTRACE_SINGLESTEP.
         * CONT plus instruction breakpoints lets real data watchpoints fire.
         * Only the hardware instruction sites observed in the first pass
         * are patched, in the child alone; all C control flow still runs. */
        for (i = 0; i < hardware_site_count; ++i) {
            uint32_t address = hardware_sites[i];
            uint32_t instruction = *(const uint32_t *)(uintptr_t)address;
            if (trace_call(4, child, address, (instruction & ~255u) | 0xcc) < 0) goto failed;
        }
        /* One-byte watches also cover every unaligned DWORD access. */
        if (trace_call(6, child, 280, 0x33330055) < 0) goto failed;
    }
    if (trace_call(7, child, 0, 0) < 0) goto failed;
    while (wait_child(child, &status)) {
        struct registers r;
        const uint8_t *code;
        uint32_t debug = 0, size = 0, expected_return;
        if (status != 0x57f || ++steps > 240 ||
            trace_call(12, child, 0, (uint32_t)(uintptr_t)&r) < 0 ||
            trace_call(3, child, 276, (uint32_t)(uintptr_t)&debug) < 0) goto failed;
        last_registers = r;
        for (i = 0; i < 4; ++i) if (debug & (1u << i)) {
            ++reads[i];
            if (mutation && reads[i] == 1) {
                if (i == 0) WORD(watches[i]) = 0x603400;
                if (i == 1) WORD(watches[i]) = 0x89abcdef;
                if (i == 2 && context_mode == 2) WORD(watches[i]) = mutation == 2 ? 0 : 0x603210;
            }
        }
        if (trace_call(6, child, 276, 0) < 0) goto failed;
        if (r.eip == (uint32_t)(uintptr_t)&after_fpu_call + 1) {
            expected_return = feature && !status_word;
            if ((function_index < 2 && r.eax != expected_return) ||
                (function_index >= 5 && r.eax != output_address) ||
                r.esp != 0x50ffec || r.ebx != 0x435a6977 || r.ebp != 0x546b7a87 ||
                r.esi != 0x657c8b97 || r.edi != 0x768d9ca7 ||
                ((r.flags ^ call_flags) & 0x200400u)) goto failed;
            if (watched_phase && (function_index == 2 || function_index == 4)) {
                if (reads[0] != 1 || reads[1] != 1 ||
                    reads[2] != (context_mode ? 2u : 0u) ||
                    reads[3] != (context_mode == 1 ? 1u : 0u)) goto failed;
            } else for (i = 0; i < 4; ++i) if (reads[i]) goto failed;
            *result = r;
            r.flags &= ~0x100u;
            if (trace_call(13, child, 0, (uint32_t)(uintptr_t)&r) < 0 ||
                trace_call(7, child, 0, 0) < 0 || !wait_child(child, &status) || status) goto failed;
            return 1;
        }
        if (watched_phase && hardware_site(r.eip - 1, 0)) --r.eip;
decode_hardware:
        size = 0;
        code = (const uint8_t *)(uintptr_t)r.eip;
        if (code[0] == 0x0f && code[1] == 0xa2) {
            if (r.eax != 1 || !event(1, r.eax, 0)) goto failed;
            r.eax = 0xfedcba98; r.ebx = 0x76543210; r.ecx = 0xabcdef00;
            r.edx = 0x87654320 | feature; size = 2;
        } else if (code[0] == 0x0f && (code[1] == 0x20 || code[1] == 0x22)) {
            if ((code[2] & 0xf8) != 0xc0) goto failed;
            if (code[1] == 0x20) {
                if (!event(2, cr0, 0)) goto failed;
                *reg(&r, code[2] & 7) = cr0;
            } else {
                cr0 = *reg(&r, code[2] & 7);
                if (!event(3, cr0, 0)) goto failed;
            }
            size = 3;
        } else if (code[0] == 0xdb && (code[1] == 0xe3 || code[1] == 0xe2)) {
            if (!event(code[1] == 0xe3 ? 4 : 6, 0, 0)) goto failed;
            if (mutation && code[1] == 0xe3 && (function_index == 2 || function_index == 4))
                WORD(0x1b164) = 0xfedcba98;
            size = 2;
        } else if (code[0] == 0xdd && code[1] == 0x7d) {
            uint32_t address = r.ebp + (int8_t)code[2], saved;
            if (!event(5, status_word, 0) ||
                trace_call(2, child, address, (uint32_t)(uintptr_t)&saved) < 0 ||
                trace_call(5, child, address, (saved & 0xffff0000) | status_word) < 0) goto failed;
            size = 3;
        } else if (code[0] == 0xdd && (code[1] == 0x30 || code[1] == 0x20)) {
            if (!event(code[1] == 0x30 ? 10 : 11, r.eax, 0)) goto failed;
            size = 2;
        } else if ((code[0] == 0x9b && code[1] == 0xd9 && code[2] == 0x30) ||
                   (code[0] == 0xd9 && code[1] == 0x20)) {
            if (!event(code[0] == 0x9b ? 12 : 13, r.eax, 0)) goto failed;
            size = code[0] == 0x9b ? 3 : 2;
        } else if (code[0] == 0x0f && code[1] == 0xae) {
            uint32_t kind = code[2] == 0 ? 14 : code[2] == 8 ? 15 : code[2] == 0x20 ? 16 : 17;
            if ((code[2] != 0 && code[2] != 8 && code[2] != 0x20 && code[2] != 0x28) ||
                !event(kind, r.eax, kind >= 16 ? r.edx : 0)) goto failed;
            size = 3;
        }
        if (size) {
            if (!hardware_site(r.eip, !watched_phase)) goto failed;
            r.eip += size;
            if (trace_call(13, child, 0, (uint32_t)(uintptr_t)&r) < 0) goto failed;
            /* Adjacent hardware instructions must each be supplied before
             * SINGLESTEP executes the next ordinary guest instruction. */
            goto decode_hardware;
        }
        if (trace_call(watched_phase ? 7 : 9, child, 0, 0) < 0) goto failed;
    }
failed:
    syscall3(37, child, 9, 0); (void)wait_child(child, &status);
    PRINT("FPU mismatch case/function/original/steps/status/EIP/EAX: ");
    print_number(cases); PRINT("/"); print_number(function_index); PRINT("/");
    print_number((uint32_t)original); PRINT("/"); print_number(steps); PRINT("/");
    print_number((uint32_t)status); PRINT("/"); print_number(last_registers.eip); PRINT("/");
    print_number(last_registers.eax); PRINT("\nFPU watch counts: ");
    for (i = 0; i < 4; ++i) { print_number(reads[i]); PRINT("/"); }
    PRINT("\n"); return 0;
}
static int oracle_event(uint32_t *at, uint32_t kind, uint32_t a, uint32_t b)
{
    uint32_t i = (*at)++;
    return i < event_count && events[i][0] == kind && events[i][1] == a && events[i][2] == b;
}
static int oracle(void)
{
    uint32_t i = 0, control = initial_cr0;
    if (function_index < 2) {
        if (!oracle_event(&i, 1, 1, 0)) return 0;
        if (feature) {
            if (!oracle_event(&i, 2, control, 0)) return 0;
            control &= ~12u;
            if (!oracle_event(&i, 3, control, 0) || !oracle_event(&i, 4, 0, 0) ||
                !oracle_event(&i, 5, status_word, 0)) return 0;
            if (function_index == 1 && !status_word) {
                if (!oracle_event(&i, 2, control, 0)) return 0;
                control = (control | 32u) & ~12u;
                if (!oracle_event(&i, 3, control, 0)) return 0;
            }
        }
    } else if (function_index < 5) {
        if (!oracle_event(&i, 2, control, 0)) return 0;
        control = function_index == 3 ? control | 12u : control & ~12u;
        if (!oracle_event(&i, 3, control, 0)) return 0;
        if (function_index != 3) {
            if (context_mode == 1) {
                if (!oracle_event(&i, 6, 0, 0) || !oracle_event(&i, 4, 0, 0)) return 0;
                if (WORD(entry_address + 32) != WORD(0x1b164) + task_index * 512u) return 0;
            } else if (context_mode == 2) {
                uint32_t context = !mutation ? 0x602001 : mutation == 2 ? 0 : 0x603210;
                if (!oracle_event(&i, 15, context, 0)) return 0;
            }
        }
    } else if (!oracle_event(&i, 10 + function_index - 5, output_address,
                             function_index >= 11 ? 0x32495867 : 0)) return 0;
    return i == event_count && cr0 == control;
}
static int check_case(void)
{
    struct registers original, rebuilt;
    uint32_t i, j;
    if (!execute_case(1, &original) || !oracle()) {
        PRINT("FPU original oracle failed at case/function/events/cr0: ");
        print_number(cases); PRINT("/"); print_number(function_index); PRINT("/");
        print_number(event_count); PRINT("/"); print_number(cr0); PRINT("\n");
        for (i = 0; i < event_count; ++i) {
            for (j = 0; j < 3; ++j) { print_number(events[i][j]); PRINT("/"); }
            PRINT("\n");
        }
        return 0;
    }
    for (i = 0; i < sizeof(expected_low); ++i) expected_low[i] = LOW[i];
    for (i = 0; i < sizeof(expected_data); ++i) expected_data[i] = DATA[i];
    expected_count = event_count; expected_cr0 = cr0;
    for (i = 0; i < event_count; ++i)
        for (j = 0; j < 3; ++j) expected_events[i][j] = events[i][j];
    if (!execute_case(0, &rebuilt) || !oracle()) { PRINT("FPU rebuilt oracle failed\n"); return 0; }
    if (cr0 != expected_cr0 || event_count != expected_count) return 0;
    for (i = 0; i < event_count; ++i)
        for (j = 0; j < 3; ++j) if (events[i][j] != expected_events[i][j]) return 0;
    for (i = 0; i < sizeof(expected_low); ++i) if (expected_low[i] != LOW[i]) return 0;
    for (i = 0; i < sizeof(expected_data); ++i) if (expected_data[i] != DATA[i]) return 0;
    ++cases; return 1;
}
/* Native x87/SSE state comparison against direct ISA instructions. The
 * i386 test compiler never allocates SSE registers; only this setup does. */
static uint8_t native_state[512] __attribute__((aligned(16)));
static uint8_t expected_native_state[512];
__attribute__((noinline)) static void native_setup(uint32_t pattern)
{
    uint16_t control = 0x37f | ((pattern & 3) << 10);
    uint32_t mxcsr = 0x1f80 | ((pattern & 3) << 13);
    uint32_t xmm[8];
    uint32_t i;
    for (i = 0; i < 8; ++i) xmm[i] = (pattern + i) * 0x1234567u;
    __asm__ volatile ("fninit\n\tfldcw %0\n\tfld1\n\tfldpi\n\tfldl2e\n\t"
                      "ldmxcsr %1\n\tmovups %2,%%xmm0\n\tmovups %3,%%xmm1"
                      : : "m" (control), "m" (mxcsr), "m" (xmm[0]), "m" (xmm[4]) : "memory");
}
__attribute__((naked, noinline)) static uint32_t native_invoke(
    uint32_t entry __attribute__((unused)), uint32_t address __attribute__((unused)))
{
    __asm__("pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\t"
            "pushl 12(%ebp)\n\txorl %edx,%edx\n\tcall *8(%ebp)\n\taddl $4,%esp\n\t"
            "popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tleave\n\tret");
    __builtin_unreachable();
}
__attribute__((noinline)) static void native_reference(uint32_t function)
{
    switch (function) {
        case 5: __asm__ volatile ("fnsave (%%eax)" : : "a" (0x600000) : "memory"); break;
        case 6: __asm__ volatile ("frstor (%%eax)" : : "a" (0x600000) : "memory"); break;
        case 7: __asm__ volatile ("fstenv (%%eax)" : : "a" (0x600000) : "memory"); break;
        case 8: __asm__ volatile ("fldenv (%%eax)" : : "a" (0x600000) : "memory"); break;
        case 9: __asm__ volatile ("fxsave (%%eax)" : : "a" (0x600000) : "memory"); break;
        case 10: __asm__ volatile ("fxrstor (%%eax)" : : "a" (0x600000) : "memory"); break;
        case 11: __asm__ volatile ("xsave (%%eax)" : : "a" (0x600000), "d" (0) : "memory"); break;
        case 12: __asm__ volatile ("xrstor (%%eax)" : : "a" (0x600000), "d" (0) : "memory"); break;
    }
}
static int native_tests(void)
{
    uint32_t cpuid[4], xsave = 0, count = 0, pattern, phase, i, address;
    kernel_cpuid_features(cpuid);
    if ((cpuid[3] & 0x1000001u) != 0x1000001u) {
        PRINT("Native FPU state comparisons skipped: host lacks FPU/FXSR\n"); return 1;
    }
    if ((cpuid[2] & 0xc000000u) == 0xc000000u) {
        uint32_t low, high;
        __asm__ volatile ("xgetbv" : "=a" (low), "=d" (high) : "c" (0));
        /* The original leaf uses EAX=buffer as its low requested mask.
         * Use the genuine zero effective mask here, not a corrected ABI. */
        xsave = !(low & 0x600000u);
        (void)high;
    }
    for (function_index = 5; function_index < (xsave ? 13u : 11u); ++function_index)
        for (pattern = 0; pattern < 16; ++pattern) {
            for (phase = 0; phase < 3; ++phase) {
                for (i = 0; i < 4096; ++i) DATA[i] = 0;
                for (i = 0; i < 512; ++i) native_state[i] = 0;
                /* Create a valid, nontrivial state image for the restore leaves. */
                native_setup(pattern);
                if (function_index == 6) native_reference(5);
                if (function_index == 8) native_reference(7);
                if (function_index == 10) native_reference(9);
                native_setup(pattern ^ 3);
                if (!phase) native_reference(function_index);
                else {
                    address = entries[function_index] + 0x100000;
                    if (phase == 2) {
#ifdef KERNEL_LINKED_TEST
                        address = entries[function_index];
#else
                        address = functions[function_index];
#endif
                    }
                    if (native_invoke(address, 0x600000) != 0x600000) goto failed;
                }
                __asm__ volatile ("fxsave %0" : "=m" (native_state) : : "memory");
                for (i = 0; i < 4096; ++i) {
                    if (!phase) expected_data[i] = DATA[i];
                    else if (expected_data[i] != DATA[i]) goto failed;
                }
                for (i = 0; i < 512; ++i) {
                    if (!phase) expected_native_state[i] = native_state[i];
                    else if (expected_native_state[i] != native_state[i]) goto failed;
                }
            }
            ++count;
        }
    __asm__ volatile ("fninit" : : : "memory");
    PRINT("FPU native state original/C/ISA comparisons: "); print_number(count); PRINT("\n");
    if (!xsave) PRINT("Native XSAVE/XRSTOR skipped: host lacks compatible zero-mask configuration\n");
    return 1;
failed:
    PRINT("FPU native mismatch function/pattern/phase/byte: ");
    print_number(function_index); PRINT("/"); print_number(pattern); PRINT("/");
    print_number(phase); PRINT("/"); print_number(i); PRINT("\n");
    return 0;
}
static int run_test(void)
{
    uint32_t c, s, flags, index, address;
    static const uint32_t sizes[] = {7, 7, 8, 7, 8, 8, 8, 8};
    if (!map_memory(0x1b000, 0x1000, 3, 0x31) ||
        !map_memory(0x500000, 0x10000, 3, 0x32) ||
        !map_memory(0x600000, 0x4000, 3, 0x31) ||
        !map_memory(0x800000, 0x40000, 7, 0x32) ||
        !map_memory(0x900000, 0x40000, 7, 0x32) ||
        !load_kernel("build/icestar.x", 0x900000) ||
        !load_kernel("build/icestar_c.x", 0x800000)) return 1;
    for (function_index = 5; function_index < 13; ++function_index)
        for (index = 0; index < sizes[function_index - 5]; ++index) {
#ifdef KERNEL_LINKED_TEST
            address = entries[function_index];
#else
            address = functions[function_index];
#endif
            if (*(uint8_t *)(uintptr_t)(address + index) !=
                *(uint8_t *)(uintptr_t)(entries[function_index] + 0x100000 + index)) return 2;
        }
    output_address = 0x602000;
    for (function_index = 0; function_index < 2; ++function_index)
        for (c = 0; c < COUNT(words); ++c)
            for (feature = 0; feature < 2; ++feature)
                for (s = 0; s < COUNT(statuses); ++s)
                    for (flags = 0; flags < 4; ++flags) {
                        initial_cr0 = words[c]; status_word = statuses[s];
                        call_flags = (flags & 1 ? 0xe93 : 0x246) | (flags & 2 ? 0x200000 : 0);
                        if (!check_case()) return 10;
                    }
    PRINT("FPU probe/enable original/C/oracle cases: "); print_number(cases); PRINT("\n"); cases = 0;
    for (function_index = 2; function_index < 5; ++function_index)
        for (c = 0; c < COUNT(words); ++c)
            for (index = 0; index < COUNT(indices); ++index)
                for (context_mode = 0; context_mode < 3; ++context_mode)
                    {
                        initial_cr0 = words[c]; task_index = indices[index];
                        call_flags = (index & 1 ? 0xe93 : 0x246) | (index & 2 ? 0x200000 : 0);
                        if (!check_case()) return 11;
                    }
    PRINT("FPU lazy-task original/C/oracle cases: "); print_number(cases); PRINT("\n"); cases = 0;
    for (function_index = 5; function_index < 13; ++function_index)
        for (address = 0; address < COUNT(words); ++address)
            for (flags = 0; flags < 4; ++flags) {
                output_address = words[address]; initial_cr0 = words[(address + 3) % COUNT(words)];
                call_flags = (flags & 1 ? 0xe93 : 0x246) | (flags & 2 ? 0x200000 : 0);
                if (!check_case()) return 12;
            }
    PRINT("FPU byte-exact hardware-boundary original/C/oracle cases: "); print_number(cases); PRINT("\n");
    cases = 0; watched_phase = 1;
    for (function_index = 2; function_index < 5; ++function_index)
        for (c = 0; c < COUNT(words); ++c)
            for (index = 0; index < COUNT(indices); ++index)
                for (context_mode = 0; context_mode < 3; ++context_mode)
                    for (mutation = 0; mutation < 3; ++mutation) {
                        initial_cr0 = words[c]; task_index = indices[index];
                        call_flags = (index & 1 ? 0xe93 : 0x246) | (index & 2 ? 0x200000 : 0);
                        if (!check_case()) return 13;
                    }
    PRINT("FPU hardware-watchpoint mutation original/C/oracle cases: "); print_number(cases); PRINT("\n");
    return native_tests() ? 0 : 14;
}
void _start(void) { finish((uint32_t)run_test()); }
