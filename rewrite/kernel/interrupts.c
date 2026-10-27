#include "interrupts.h"
#define WORD(address) (*(volatile uint32_t *)(uintptr_t)(address))

/* 0x813320..0x8135c6. Capture the boot code selector once. Vector 255 and
 * unused callback slots deliberately retain their previous contents. */
void kernel_init_interrupts(void)
{
    uint32_t selector = kernel_boot_code_selector(), vector;
    for (vector = 0; vector < 32; ++vector)
        kernel_install_interrupt_gate(0x8371a0, selector, 0x8e00, vector);
    for (vector = 33; vector < 255; ++vector)
        kernel_install_interrupt_gate(0x8371bc, selector, 0x8e00, vector);
#define GATE(vector_, handler_, attributes_) \
    kernel_install_interrupt_gate(handler_, selector, attributes_, vector_)
    GATE(7, 0x8371e6, 0x8e00);
    GATE(32, 0x8371e9, 0x8e00);
    GATE(33, 0x837209, 0x8e00);
    GATE(37, 0x837222, 0x8e00);
    GATE(40, 0x83723b, 0x8e00);
    GATE(41, 0x83725a, 0x8e00);
    GATE(42, 0x837273, 0x8e00);
    GATE(43, 0x83728c, 0x8e00);
    GATE(44, 0x8372a5, 0x8e00);
    GATE(64, 0x8372be, 0xee00);
    GATE(65, 0x8372d1, 0xee00);
#undef GATE
    WORD(0x18600) = 0x81364c;
    WORD(0x1861c) = 0x8380e1;
    WORD(0x18680) = 0x813a5e;
    WORD(0x18684) = 0x813aa6;
    WORD(0x18694) = 0x813ae6;
    WORD(0x186a0) = 0x813a8c;
    WORD(0x186a4) = 0x813b2a;
    WORD(0x186a8) = 0x813b2a;
    WORD(0x186ac) = 0x813b2a;
    WORD(0x186b0) = 0x813b30;
    WORD(0x18700) = 0x813b70;
    WORD(0x18704) = 0x81406c;
}
