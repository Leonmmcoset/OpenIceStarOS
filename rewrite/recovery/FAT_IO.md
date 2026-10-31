# FAT contiguous-sector writer

The body at `0x827c72..0x827cc3` is now recovered in
`kernel/fat_io.c`. It computes the end LBA with 32-bit wrapping arithmetic,
loops while the current LBA is below that end using an unsigned comparison,
and calls the original ATA one-sector entry at `0x828c18` once per sector.
The source advances by the live BPB bytes-per-sector word at
`0x52000b` after each call. The hardware entry issues ATA WRITE SECTORS
(`0x30`) and sends 256 words through port `0x1f0` with `out dx,ax`. The ATA
return value is ignored, matching the original caller contract. Earlier
notes called this a reader; the command and data direction establish that it
is a writer. The hardware entry itself remains original assembly.

`tests/kernel_fat_io.py` intercepts the hardware entry and compares original
x86, ordinary linked C, and fixed-address C. It covers zero and wrapped
ranges, live BPB stride changes during a transfer, call order, source
addresses, helper return values, source bytes, source-buffer preservation,
and the cdecl register/stack contract.
The installed run passes 127 scenarios and 381 executions; the compiled body
is 50 of its 81 available slot bytes.

```sh
make -C rewrite build/openicestaros_kernel.x test-fat-io
make -C rewrite inventory-check
```

This is a focused FAT I/O recovery step. The kernel, INIT, drivers, and most
applications still contain original or partial code, so it does not complete
the OS rewrite.
