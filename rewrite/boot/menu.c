typedef unsigned char u8;
typedef unsigned short u16;

#ifndef NEXT_STAGE_SECTORS
#define NEXT_STAGE_SECTORS 4
#endif
#define STRINGIFY_INNER(x) #x
#define STRINGIFY(x) STRINGIFY_INNER(x)

static void put_cell(u16 index, u8 character, u8 attribute)
{
    u16 value = (u16)character | ((u16)attribute << 8);
    __asm__ volatile (
        "pushw %%es\n\t"
        "movw $0xb800, %%ax\n\t"
        "movw %%ax, %%es\n\t"
        "movw %0, %%es:(%1)\n\t"
        "popw %%es"
        : : "r"(value), "r"((unsigned int)index * 2) : "eax", "memory");
}

static void draw_line(u16 row, const char *text, u8 attribute)
{
    u16 column;
    u8 character = 1;
    for (column = 0; column < 40; ++column) {
        if (character)
            character = (u8)*text++;
        put_cell(row * 80 + column, character ? character : ' ', attribute);
    }
}

static void draw_menu(u8 selected)
{
    draw_line(0, "Please select a system startup", 7);
    draw_line(2, "      1.IceStarOS", selected == 1 ? 0x17 : 7);
    draw_line(3, "      2.IceStarOS(1024*768 8bit)", selected == 2 ? 0x17 : 7);
    draw_line(4, "      3.Reboot Computer", selected == 3 ? 0x17 : 7);
    draw_line(5, "      4.off Computer", selected == 4 ? 0x17 : 7);
    draw_line(7, "Choose:[1]", 7);
    put_cell(7 * 80 + 8, '0' + selected, 7);
}

static u16 read_key(void)
{
    u16 key;
    __asm__ volatile (
        "movb $1, %%ah\n\t"
        "int $0x16\n\t"
        "jz 1f\n\t"
        "xorb %%ah, %%ah\n\t"
        "int $0x16\n\t"
        "jmp 2f\n"
        "1: movw $0xffff, %%ax\n"
        "2:"
        : "=a"(key) : : "cc");
    return key;
}

static u16 clock_ticks(void)
{
    u16 high, low;
    __asm__ volatile ("xorw %%ax, %%ax; int $0x1a"
                      : "=c"(high), "=d"(low) : : "eax", "cc");
    (void)high;
    return low;
}

static __attribute__((noreturn)) void boot_next(u8 selected)
{
    /* Read the original MBR's boot-drive byte, then the stage at CHS 0/0/6. */
    __asm__ volatile (
        "xorw %%ax, %%ax\n\t"
        "movw %%ax, %%es\n\t"
        "movb %%es:0x7c02, %%dl\n\t"
        "movw $0x1a80, %%ax\n\t"
        "movw %%ax, %%es\n\t"
        "xorw %%bx, %%bx\n\t"
        "movw $(0x0200 + " STRINGIFY(NEXT_STAGE_SECTORS) "), %%ax\n\t"
        "movw $0x0006, %%cx\n\t"
        "xorb %%dh, %%dh\n\t"
        "pushw %%si\n\t"
        "int $0x13\n\t"
        "popw %%si\n\t"
        "movw $0x1a80, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "cmpw $2, %w0\n\t"
        "jne 1f\n\t"
        "movw $0x0105, 0x000a\n"
        "1: ljmpw *0x0004"
        : : "S"((unsigned int)selected) : "eax", "ebx", "ecx", "edx", "memory", "cc");
    __builtin_unreachable();
}

static __attribute__((noreturn)) void shutdown(void)
{
    __asm__ volatile (
        "movw $0x5301, %%ax; xorw %%bx, %%bx; int $0x15\n\t"
        "movw $0x530e, %%ax; xorw %%bx, %%bx; movw $0x0102, %%cx; int $0x15\n\t"
        "movw $0x5307, %%ax; movw $1, %%bx; movw $3, %%cx; int $0x15"
        : : : "eax", "ebx", "ecx", "memory", "cc");
    for (;;) { }
}

__attribute__((noreturn)) void menu_main(void)
{
    u8 selected = 1, remaining = 10;
    u16 last = clock_ticks();
    __asm__ volatile ("movw $0x0600, %%ax; movw $0x0700, %%bx;"
                      "xorw %%cx, %%cx; movw $0x184f, %%dx; int $0x10"
                      : : : "eax", "ebx", "ecx", "edx", "memory", "cc");
    draw_menu(selected);
    while (remaining) {
        u16 now = clock_ticks();
        u16 key = read_key();
        if ((u16)(now - last) >= 18) {
            last = now;
            --remaining;
            put_cell(7 * 80 + 18, '0' + remaining, 7);
        }
        if (key == 0xffff)
            continue;
        if ((u8)key >= '1' && (u8)key <= '4')
            selected = (u8)key - '0';
        else if (key == 0x5000)
            selected = selected == 4 ? 1 : selected + 1;
        else if (key == 0x4800)
            selected = selected == 1 ? 4 : selected - 1;
        else if ((u8)key == 13)
            break;
        else
            continue;
        draw_menu(selected);
    }
    if (selected <= 2)
        boot_next(selected);
    if (selected == 3)
        __asm__ volatile ("ljmp $0xffff, $0x0000");
    shutdown();
}
