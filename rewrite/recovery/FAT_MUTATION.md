# FAT chain release and directory deletion

Four more original-only entries now execute recovered C in
kernel/fat_mutation.c. They keep their original fixed addresses and cdecl ABI.

| Address | Operation | Slot bytes | C bytes |
| --- | --- | ---: | ---: |
| 0x827d4a | Release a root record's FAT chains | 290 | 189 |
| 0x827e6c | Release a supplied record's FAT chains | 236 | 127 |
| 0x827f58 | Mark a root record deleted and write metadata | 138 | 85 |
| 0x827fe2 | Find a relative name and delete its root record | 54 | 30 |

The release routines cache both FAT pointers before traversing either copy.
They combine record words +20 and +26 into the starting cluster and read the
next FAT16 value before zeroing each entry. Only next values zero and exactly
0xffff stop traversal. Values such as 0xfff8 remain indices, including the
original unchecked access outside the declared FAT size. Initial clusters
zero/0xffff still receive one clear; cycles eventually encounter a cleared
entry. At most two FAT buffers are processed even when the BPB count exceeds
two. The second-copy enable check, starting record fields, and returned FAT
count are live reads. The original index/address arithmetic wraps at 32 bits.

Deletion is separate from releasing clusters. It rejects only directory
first bytes zero/0xe5, otherwise writes 0xe5 before invoking metadata
writeback, ignores writeback's EAX, and returns one. It does not clear a FAT
chain, the rest of the directory entry, or file data. The relative wrapper
requires a signed positive lookup result: index zero and negative results
return -1 without deleting anything. No validation or policy fixes have
been added to the recovered behavior.

## Differential and production integration checks

tests/kernel_fat_mutation.py compares original x86, ordinary C, and installed
fixed-address C in 472 scenarios / 1,416 executions. Only name lookup and
metadata writeback are intercepted. Checks include ordered FAT/record writes,
complete 2 MiB fixture memory, flush-time record state, return values,
callee-saved registers and caller stack. Synthetic cases cover all 256 first
bytes, signed lookup limits, wrapping/high cluster indices, independent and
aliased FAT buffers, cycles, exact sentinels, and mutations between FATs.
Both original FAT copies for all 39 program paths supply corpus cases.

The linked production kernel contains 395 declared intervals. These focused
regressions also pass:

| Suite | Scenarios | Executions |
| --- | ---: | ---: |
| Metadata writeback | 1,924 | 5,772 |
| Contiguous sector writer | 127 | 381 |
| Free-slot/cluster scans | 208 | 624 |
| FAT file readers | 2,410 | 7,230 |
| X loader | 430 | 1,290 |

The exact production-image and application-installation checks pass, as do
inventory-check and git diff --check. No full make verify was run for this
checkpoint.

## Real QEMU disk writes

tests/fat_mutation_guest.c is a test-only X application. The host test
kernel_fat_mutation_qemu.py temporarily replaces COLOR.X with this diagnostic
and creates two root fixture records, MUTROOT.TST and MUTREC.TST. Their
separate two-cluster chains use four otherwise free clusters. The five
production application registrations and recovery statuses are unchanged.

The same diagnostic runs under the original kernel and the installed C
kernel, launched through CMD using emulated PS/2 input. It calls both chain
release entries, both deletion entries, failure paths for already deleted
records, and standalone metadata writeback. There are no intercepted
filesystem helpers and no debugger calls or guest-state injection. Trace
evidence observes the six C entry/body ranges and original ATA writer.
QEMU receives only disposable writable copies of the input images.

Both runs return identical reports and change exactly 18 disk bytes: four
FAT words in each FAT and the two directory first bytes. Each entire
33,554,944-byte resulting disk equals an independently constructed expected
image, including unchanged fixture data, boot sectors, other files, slack,
and VHD footer. Input images are checked unchanged after execution. The
privileged ATA routine at 0x828c18 still uses original assembly.

    make -C rewrite build/OpenIceStarOS_c_apps.vhd \
      test-fat-mutation test-fat-metadata test-fat-io \
      test-fat-slots test-fat-files test-x-loader
    make -C rewrite test-fat-mutation-qemu
    python3 rewrite/tools/kernel_image.py verify IceStarOS_x86_32M.vhd \
      rewrite/build/OpenIceStarOS_c_init.vhd rewrite/build/openicestaros_kernel.x \
      rewrite/kernel/patches.json rewrite/build
    python3 rewrite/tools/app_image.py verify rewrite/build/OpenIceStarOS_c_init.vhd \
      rewrite/apps/images.json rewrite/build rewrite/build/OpenIceStarOS_c_apps.vhd
    python3 rewrite/tools/boot_smoke.py rewrite/build/OpenIceStarOS_c_apps.vhd \
      --clock-check --memory-check --task-check
    make -C rewrite inventory-check

Ordinary production boot also verifies CMD/cpuid, C CLOCK launch/animation/
close, RAM/heap, timer/TSS switching, and all 395 installed intervals. Small
machine-readable summaries are committed in
../evidence/2026-10-31-fat-mutation/. Images, traces and memory dumps stay in
ignored build/. These checks cover the recovered operations on the fixtures;
they do not establish arbitrary file-writing or full filesystem correctness.

The original logo remains unchanged. INIT/kernel remain partial, five
applications are rebuilt and 32 program paths remain unrecovered. The build
still needs original machine code; the complete OS recovery remains open.
