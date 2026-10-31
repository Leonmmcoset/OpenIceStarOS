# FAT/root metadata writeback

`kernel/fat_metadata.c` recovers `0x827cc4..0x827d49` in C. The routine
caches the FAT buffer pointer at `0x1a83a`, computes the first FAT disk LBA
as partition start plus BPB reserved sectors, and writes FAT copies followed
by the root directory through `0x827c72`. The root sector count is
`root_entries * 32 / bytes_per_sector`, using signed division without rounding
up. The cached total is returned even if the writer changes EAX or globals.
All address additions retain the original 32-bit wrap behavior. A zero BPB
sector size still raises an x86 divide exception.

The downstream entry at `0x828c18` is an ATA writer (`0x30`, `out dx,ax`),
not a reader. The contiguous writer is recovered C; the privileged ATA entry
remains original assembly. This does not recover the remaining file-writing
or deletion logic.

`tests/kernel_fat_metadata.py` compares original x86, ordinary C, and
fixed-address C for 1,924 scenarios / 5,772 executions. The test checks
independent expected arguments, source bytes and source preservation,
partition wrapping, BPB extremes, unrounded root counts, ignored helper
return values, cached globals, division faults, and cdecl stack/registers.
Only the downstream writer is intercepted. The installed run compares the
101-byte C body against the production kernel's 133-byte slot.

```sh
make -C rewrite build/OpenIceStarOS_c_apps.vhd test-fat-metadata test-fat-io
python3 rewrite/tools/kernel_image.py verify IceStarOS_x86_32M.vhd \
  rewrite/build/OpenIceStarOS_c_init.vhd rewrite/build/openicestaros_kernel.x \
  rewrite/kernel/patches.json rewrite/build
python3 rewrite/tools/app_image.py verify rewrite/build/OpenIceStarOS_c_init.vhd \
  rewrite/apps/images.json rewrite/build rewrite/build/OpenIceStarOS_c_apps.vhd
python3 rewrite/tools/boot_smoke.py rewrite/build/OpenIceStarOS_c_apps.vhd \
  --input-check --memory-check --task-check
make -C rewrite inventory-check
```

The production-image disk checks pass. QEMU boot validates input/cpuid,
RAM/heap, timer/TSS scheduling and all 391 installed intervals. Normal boot
does not exercise this writeback entry; its behavior is covered by the
focused differential suite. No full `make verify` was run at this checkpoint.
INIT/kernel remain partial, five applications are rebuilt and 32 program
paths remain unrecovered.
