# FAT directory query and relative lookup recovery

`kernel/fat_directory.c` recovers ten previously original-only kernel services:

| Address | Service | Operation | Original slot bytes | Compiled C bytes |
| --- | ---: | --- | ---: | ---: |
| `0x8258a6` | 114 | Root record to displayed filename | 472 | 329 |
| `0x825a7e` | 119 | Arbitrary directory record to displayed filename | 334 | 209 |
| `0x825bcc` | 207 | Root record extension | 122 | 99 |
| `0x825c46` | 208 | Arbitrary directory record extension | 72 | 40 |
| `0x825c8e` | 122 | Find a relative file in the loaded root | 166 | 88 |
| `0x825d34` | 123 | Find a file in a supplied directory buffer | 192 | 113 |
| `0x826940` | 201 | Root record file size | 76 | 58 |
| `0x82698c` | 202 | Arbitrary directory record file size | 26 | 20 |
| `0x8269a6` | 115 | Root record attributes | 76 | 59 |
| `0x8269f2` | 120 | Arbitrary directory record attributes | 28 | 21 |

The root base is recalculated from the loaded BPB at `0x520000`. Other entries
use the supplied base and wrapping 32-bit `index * 32` without new validation.
Name extraction returns 0 for an end marker, -1 for deleted records, and -2 for
exact attribute `0x0f`. Only the root-name entry returns -3 for a volume label,
after writing its displayed name. The other name entry returns 1 for a volume
label. Both retain their bytewise basename/extension rules, early space
termination and special handling of an eighth basename byte that is a dot.
They do not uppercase, implement long names or normalize malformed records.
Extension getters always copy three raw bytes and a NUL, return the original
output pointer, and do not reject deleted/end/LFN records. Size and attribute
getters likewise perform raw reads without checking record validity.

Relative lookup compares displayed names case-sensitively, rejects volume and
directory attribute bits, returns the matching index including zero, and
returns -1 on failure. It skips deleted/LFN records and stops at an end marker.
The root loop rereads the BPB root-entry bound on every iteration; the supplied
directory loop caches one cluster's entry bound before scanning. Name and
attribute calls remain explicit, including the fresh root-base calculation
when reading attributes after string comparison. These services do not read
hardware sectors or traverse a directory FAT chain themselves.

## Source validation

```sh
python3 rewrite/tests/kernel_fat_directory.py
```

The preview passes **5,974 scenarios / 17,922 executions** across original x86,
ordinary C and independently linked fixed-address C. No service is stubbed:
name extraction, string equality, attribute access and lookup execute as x86.
Code hooks record ordered service calls and apply declared memory mutations
at comparison entry; they never supply return values or skip instructions.

The original disk contributes 12 directories and 101 occupied records,
including root and child directory data for all 67 files. Independent checks
cover displayed names, raw extensions, file sizes, attributes and lookup
indices for valid corpus records. Synthetic cases cover all 256 attributes,
case sensitivity, name/extension padding and dot rules, marker status returns
and untouched outputs, high-bit bytes, 32-bit sizes and index wraparound,
unaligned bases, bounded output/record overlaps, early terminators and scan
limits. Comparison-entry mutations check fresh attributes/root geometry,
live root-entry bounds and cached child-directory bounds. Complete checked
memory arenas, return values, ordered service calls and cdecl caller
stack/callee-saved registers agree among all three implementations.

Unmapped pointers, arbitrary stack aliases and unbounded destructive input
aliases are outside this fixture set. This recovery preserves the original
unchecked API rather than claiming new bounds guarantees. Preview evidence is
in `../evidence/2026-10-29-fat-directory/`. The source checkpoint was committed
before production installation; kernel/INIT remain partial and no new app is
marked rebuilt.

## Integrated validation

The ten C bodies are installed at their original service addresses; the kernel
now contains **378 declared intervals**. Installed bytes agree with the
independent preview link. All 5,974 scenarios / 17,922 executions pass while
using the production string-equality helper in the installed variant. Existing
reader tests pass 2,410 / 7,230, absolute-path tests 347 / 1,041, and X-loader
tests 430 / 1,290. Installed icon-directory/icon-file/bitmap-file callers,
exact kernel/application disk checks, six branding tests, residual audit and
inventory validation also pass.

```sh
make -C rewrite build/OpenIceStarOS_c_apps.vhd \
  build/OpenIceStarOS_fat_diagnostic.vhd \
  test-fat-directory test-fat-files test-fat-path test-x-loader
python3 rewrite/tools/boot_smoke.py \
  rewrite/build/OpenIceStarOS_fat_diagnostic.vhd \
  --fat-relative-check --memory-check --task-check
python3 rewrite/tools/boot_smoke.py \
  rewrite/build/OpenIceStarOS_c_apps.vhd \
  --clock-check --fat-files-check --memory-check --task-check
```

The separate diagnostic X verifies eighteen actual production service
addresses: eight readers and all ten newly recovered directory queries. Its
26 checked calls validate **525,120 data/guard bytes**, filename/extension
contents, pointer/index/size/attribute returns, relocations and allocation/free
results using real kernel services and disk I/O. Supplied-directory queries
use a separate copied directory buffer. QEMU observes every new service entry
and interior blocks for both name extractors and both lookups. The six small
getters can execute in a single translation block; their entry traces and
independently checked return/output data establish execution without requiring
an additional interior block. The diagnostic only replaces COLOR.X in its
separate image and is never treated as a recovered production application.

Ordinary production boot separately observes the new root-name entry 3 times,
directory-name entry 27 times, directory lookup once, and each attribute getter
once. The other five new entries are covered by the diagnostic and differential
tests, with no observed production-workload entry. The desktop, branded CMD
and `cpuid`, three complete changing CLOCK frames, PS/2 window close, task exit,
CMD reuse, 128 MiB RAM/heap, real TSS switching and all 378 installed guest
intervals pass. Both boots use snapshots. The committed CLOCK and CMD images
come directly from QEMU and were visually inspected.

Exactly **1,479 bytes** differ from the preceding 368-interval production
image, solely inside the ten new slots. Restoring those original baseline
kernel bytes reproduces the previous complete disk byte for byte. Boot stages,
application files, FATs/directories/slack, the retained original logo and all
other resources are unchanged; the original VHD hash is unchanged as well.
Evidence is in [`../evidence/2026-10-29-fat-directory/`](../evidence/2026-10-29-fat-directory/).

This is focused integration validation, not a full 378-interval `make verify`
run. Five applications are rebuilt; kernel/INIT remain partial and 32 program
paths remain unrecovered. Directory-only lookup variants, remaining metadata
services, hardware I/O, drivers and applications still require recovery.
