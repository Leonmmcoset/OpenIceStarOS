/* First-stage BIOS boot loader. The menu reads boot_drive at 0000:7c02. */
typedef unsigned char u8;
typedef unsigned short u16;

extern volatile u8 boot_drive;

__attribute__((noreturn)) void mbr_main(void)
{
    u16 drive = boot_drive;

    /* CHS 0/0/2, three sectors, destination 1a00:0000. */
    __asm__ volatile (
        "movw $0x1a00, %%ax\n\t"
        "movw %%ax, %%es\n\t"
        "xorw %%bx, %%bx\n\t"
        "movw $0x0203, %%ax\n\t"
        "movw $0x0002, %%cx\n\t"
        "int $0x13\n\t"
        "movw $0x1a00, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "ljmpw *0x0004\n\t"
        : : "d" (drive) : "eax", "ebx", "ecx", "memory", "cc");
    __builtin_unreachable();
}

/* Preserve the disk's partition table and signature. */
__attribute__((section(".partition"), used, aligned(1)))
const u8 partition_table[64] = {
    0x80, 0x01, 0x01, 0x00, 0x04, 0x03, 0xd1, 0xc2,
    0x11, 0x00, 0x00, 0x00, 0xbb, 0xff, 0x00, 0x00
};

__attribute__((section(".signature"), used, aligned(1)))
const u8 boot_signature[2] = { 0x55, 0xaa };
