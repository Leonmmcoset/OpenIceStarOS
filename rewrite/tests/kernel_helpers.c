/* Execute the untouched machine code, host-linked C, and installed C helpers.
 * No helper is mocked. Guard pages check exact terminator boundaries. */
#include "../kernel/helpers.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define PRINT(s) syscall3(4, 1, (unsigned)(uintptr_t)(s), sizeof(s) - 1)

static uint8_t expected_memory[0x2000];
static uint32_t input[4], path, cases, length, pattern, alignment, variant, bound;
static uint32_t character, second_length, descriptor_limit, descriptor_base, descriptor_access;
static uint32_t mismatch_address;
static const uint32_t values[] = {
    0, 1, 2, 0x38, 0x40, 0x202, 0xffff, 0x10000,
    0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff, 0xa55ac33c
};
static const uint32_t limits[] = {
    0, 1, 0xffff, 0x10000, 0xffffe, 0xfffff, 0x100000, 0x100001,
    0x100fff, 0x101000, 0x1fffff, 0x7fffffff, 0x80000000,
    0xfffff000, 0xfffff001, 0xffffffff, 0xa55ac33c
};
static const uint32_t access_values[] = {
    0, 1, 0x89, 0x92, 0xfa, 0x4000, 0x4092, 0x8000,
    0x8092, 0xc092, 0xffff, 0x10000, 0x7fffffff, 0x80000000,
    0xffffffff, 0xa55ac33c
};
static const uint32_t lengths[] = {0, 1, 2, 3, 7, 31, 63, 64, 127, 255, 511};
static const uint32_t bounds[] = {
    0, 1, 2, 3, 7, 31, 63, 64, 127, 255, 511, 512,
    0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff
};
static const uint32_t characters[] = {0, 1, 0x7f, 0x80, 0x9f, 0xa0, 0xff, 0x100, 0xffffffff};

static int syscall3(int number, unsigned a, unsigned b, unsigned c)
{
    int result;
    __asm__ volatile ("int $0x80" : "=a" (result)
                      : "a" (number), "b" (a), "c" (b), "d" (c) : "memory");
    return result;
}

static void finish(unsigned code)
{
    syscall3(1, code, 0, 0);
    __builtin_unreachable();
}

static int map_memory(uint32_t address, uint32_t size, uint32_t protection)
{
    uint32_t args[] = {address, size, protection, 0x32, 0xffffffff, 0};
    return (uint32_t)syscall3(90, (unsigned)(uintptr_t)args, 0, 0) == address;
}

static int load_kernel(const char *name, uint32_t address)
{
    int fd = syscall3(5, (unsigned)(uintptr_t)name, 0, 0), size;
    if (fd < 0) return 0;
    size = syscall3(3, (unsigned)fd, address, 0x40000);
    syscall3(6, (unsigned)fd, 0, 0);
    return size == 250099;
}

static void make_string(uint32_t address, uint32_t size)
{
    uint32_t i;
    for (i = 0; i < size; ++i) {
        uint8_t byte = (uint8_t)(33u + i % 80u);
        if (pattern == 1 || (pattern == 2 && i == 0) ||
            (pattern == 3 && i + 1u == size) ||
            (pattern == 4 && (i == 0 || i + 1u == size))) byte = 0xa0u + i % 96u;
        if (pattern == 5) byte = (uint8_t)(1u + (i * 59u) % 255u);
        BYTE(address + i) = byte;
    }
    BYTE(address + size) = 0;
}

static void initialize(void)
{
    uint32_t i, source = 0x601000 + alignment, destination = 0x600100 + alignment;
    for (i = 0; i < sizeof(expected_memory); ++i)
        BYTE(0x600000 + i) = (uint8_t)(i * 29u + cases * 71u + pattern * 17u);
    if (path == 0) {
        input[0] = alignment == 8 ? 0x601ff8 : 0x600200 + alignment;
        input[1] = descriptor_limit;
        input[2] = descriptor_base;
        input[3] = descriptor_access;
        return;
    }
    if (path <= 3) {
        if (variant == 1) destination = source - 1u;
        if (variant == 2) destination = source - 17u;
        if (variant == 3) destination = source;
        if (variant == 4) destination = source + 1u;
        if (variant == 7) source = 0x602000 - length - 1u;
        if (variant == 8) {
            source = 0x600100 + alignment;
            destination = 0x602000 - length - 1u;
        }
        make_string(source, length);
        input[0] = destination;
        input[1] = source;
        input[2] = bound;
        if (variant == 5) { input[0] = 0; input[1] = 0xffffffff; }
        if (variant == 6) { input[0] = 0xffffffff; input[1] = 0; }
        if (variant == 9) input[1] = 0xffffffff;
        return;
    }
    if (path == 4) {
        if (variant) source = 0x602000 - length - 1u;
        make_string(source, length);
        input[0] = source;
    } else if (path == 5) {
        make_string(source, length);
        make_string(destination, length);
        if (variant == 1) BYTE(source) ^= 0x55;
        if (variant == 2) BYTE(source + length / 2u) ^= 0x55;
        if (variant == 3 && length) BYTE(source + length - 1u) ^= 0x55;
        if (variant == 4) BYTE(source + length / 2u) = 0;
        if (variant == 5) {
            source = 0x602000 - length - 1u;
            make_string(source, length);
        }
        input[0] = destination;
        input[1] = source;
    } else if (path == 6) {
        make_string(destination, length);
        input[0] = destination;
        input[1] = character;
        input[2] = bound;
    } else {
        make_string(destination, length);
        make_string(source, second_length);
        input[0] = destination;
        input[1] = source;
    }
}

static uint32_t invoke(int original)
{
    static const uint32_t offsets[] = {
        0x135c6, 0x2a32b, 0x2a36d, 0x2a401, 0x2a45d, 0x2a481, 0x2a4bb, 0x2a4fb
    };
    uint32_t entry = (original ? 0x900000 : 0x800000) + offsets[path];
#ifndef KERNEL_LINKED_TEST
    if (!original) {
        if (path == 0) { kernel_set_segment_descriptor(input[0], input[1], input[2], input[3]); return 0; }
        if (path == 1) return kernel_copy_string(input[0], input[1]);
        if (path == 2) return kernel_copy_text_prefix(input[0], input[1], (int32_t)input[2]);
        if (path == 3) return kernel_copy_string_limit(input[0], input[1], input[2]);
        if (path == 4) return kernel_string_length(input[0]);
        if (path == 5) return kernel_strings_equal(input[0], input[1]);
        if (path == 6) return kernel_append_character(input[0], input[1], input[2]);
        kernel_append_string(input[0], input[1]); return 0;
    }
#endif
    if (path == 0) {
        ((void (*)(uint32_t, uint32_t, uint32_t, uint32_t))entry)(input[0], input[1], input[2], input[3]);
        return 0;
    }
    if (path == 7) { ((void (*)(uint32_t, uint32_t))entry)(input[0], input[1]); return 0; }
    if (path == 4) return ((uint32_t (*)(uint32_t))entry)(input[0]);
    if (path == 1 || path == 5) return ((uint32_t (*)(uint32_t, uint32_t))entry)(input[0], input[1]);
    return ((uint32_t (*)(uint32_t, uint32_t, uint32_t))entry)(input[0], input[1], input[2]);
}

static int compare(int save)
{
    uint32_t i;
    for (i = 0; i < sizeof(expected_memory); ++i) {
        if (save) expected_memory[i] = BYTE(0x600000 + i);
        else if (expected_memory[i] != BYTE(0x600000 + i)) {
            mismatch_address = 0x600000 + i;
            return 0;
        }
    }
    return 1;
}

static void print_number(uint32_t number)
{
    char digits[10];
    uint32_t size = 0;
    do { digits[9 - size++] = (char)('0' + number % 10); number /= 10; } while (number);
    syscall3(4, 1, (unsigned)(uintptr_t)(digits + 10 - size), size);
}

static int run_case(void)
{
    uint32_t expected, actual;
    initialize();
    expected = invoke(1);
    compare(1);
    initialize();
    actual = invoke(0);
    mismatch_address = 0;
    if (!compare(0) || actual != expected) {
        PRINT("kernel helper mismatch path/case/expected/actual/address: ");
        print_number(path); PRINT("/"); print_number(cases); PRINT("/");
        print_number(expected); PRINT("/"); print_number(actual); PRINT("/");
        print_number(mismatch_address); PRINT("\n");
        return 0;
    }
    ++cases;
    return 1;
}

static int run_test(void)
{
    uint32_t i, j, k, l, m, n;
    if (!map_memory(0x600000, 0x2000, 3) || !map_memory(0x602000, 0x1000, 0) ||
        !map_memory(0x800000, 0x40000, 7) || !map_memory(0x900000, 0x40000, 7) ||
        !load_kernel("build/icestar.x", 0x900000) ||
        !load_kernel("build/openicestaros_kernel.x", 0x800000)) return 1;
    for (path = 0; path < 8; ++path) {
        cases = 0;
        if (path == 0) {
            for (i = 0; i < COUNT(limits); ++i)
                for (j = 0; j < COUNT(values); ++j)
                    for (k = 0; k < COUNT(access_values); ++k)
                        for (alignment = 0; alignment < 9; ++alignment)
                            for (pattern = 0; pattern < 2; ++pattern) {
                                descriptor_limit = limits[i];
                                descriptor_base = values[j];
                                descriptor_access = access_values[k];
                                if (!run_case()) return 10;
                            }
        } else {
            for (i = 0; i < COUNT(lengths); ++i)
                for (pattern = 0; pattern < 6; ++pattern)
                    for (alignment = 0; alignment < 4; ++alignment) {
                        length = lengths[i];
                        if (path <= 3) {
                            for (variant = 0; variant < 10; ++variant)
                                for (j = 0; j < (path == 1 ? 1u : COUNT(bounds)); ++j) {
                                    bound = bounds[j];
                                    if (variant == 4 && (path == 1 || bound > 512u)) continue;
                                    if ((variant == 7 || variant == 8) && alignment) continue;
                                    if (variant == 9 && (path == 1 ||
                                        (path == 2 ? (int32_t)bound > 0 : bound != 0))) continue;
                                    if (!run_case()) return 10;
                                }
                        } else if (path <= 5) {
                            for (variant = 0; variant < (path == 4 ? 2u : 6u); ++variant) {
                                if (variant && path == 4 && alignment) continue;
                                if (variant == 5 && alignment) continue;
                                if (!run_case()) return 10;
                            }
                        } else if (path == 6) {
                            for (l = 0; l < COUNT(bounds); ++l)
                                for (m = 0; m < COUNT(characters); ++m) {
                                    bound = bounds[l]; character = characters[m];
                                    if (!run_case()) return 10;
                                }
                        } else {
                            for (n = 0; n < COUNT(lengths); ++n) {
                                second_length = lengths[n];
                                if (!run_case()) return 10;
                            }
                        }
                    }
        }
        PRINT("kernel helper path/cases: "); print_number(path); PRINT("/"); print_number(cases); PRINT("\n");
    }
    return 0;
}

void _start(void) { finish((unsigned)run_test()); }
