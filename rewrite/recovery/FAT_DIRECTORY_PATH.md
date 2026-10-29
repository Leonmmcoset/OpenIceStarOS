# Directory-only lookup and FAT date/time recovery

This step recovers seven previously original-only kernel entries. The six
relative/metadata routines extend `kernel/fat_directory.c`; the absolute
directory walker is in `kernel/fat_directory_path.c`.

| Address | Service | Operation | Original slot bytes | Compiled C bytes |
| --- | ---: | --- | ---: | ---: |
| `0x825df4` | 124 | Find a directory in the loaded root | 146 | 88 |
| `0x825e86` | 125 | Find a directory in a supplied buffer | 194 | 113 |
| `0x825f48` | 117 | Absolute directory path to a 32-byte record | 1144 | 473 |
| `0x826a0e` | 203 | Root record raw date word at +24 | 76 | 59 |
| `0x826a5a` | 204 | Supplied directory raw date word at +24 | 28 | 21 |
| `0x826a76` | 205 | Root record raw time word at +22 | 76 | 59 |
| `0x826ac2` | 206 | Supplied directory raw time word at +22 | 28 | 21 |

Relative lookup calls the existing name extractor, compares displayed names
case-sensitively, then requires attribute bit 16. Only extraction status 1 is
eligible; the root extractor rejects volume labels whereas the supplied-buffer
extractor does not. The first eligible match wins, including index zero.
This matters for the original root's volume label and directory both named
`ICESTAR`. Failure returns -1. Root entry limits and attributes/root geometry
remain live reads; the supplied-buffer entry caches one cluster's scan bound.

The absolute walker preserves the original 32-by-13 parser, uppercase and 8.3
conversion, drive-prefix acceptance, one trailing separator, rejection of
empty/overlong components, and silent parser cap. `C:` and `C:\` return zero
without touching output. Non-drive/missing paths return zero; malformed parser
input returns -1. Directory records require bit 16, with the original signed
short-name versus unsigned record-byte comparison. Every non-root scan is
limited to one cluster even if a directory has a longer FAT chain.

Intermediate directory buffers use the existing allocating reader. At depth
greater than one, the original releases the current directory before reading
the matched record, including before copying a final record to output. The C
version retains that ordering and original allocation-failure behavior. A
free-boundary mutation fixture verifies that the post-free record is consumed.
This is a compatibility recovery, not a correction of the original unsafe
ordering. Date/time getters return unsigned raw words, with wrapping indices,
unaligned reads and no record-validity or timestamp conversion checks.

## Source validation

```sh
python3 rewrite/tests/kernel_fat_directory.py
python3 rewrite/tests/kernel_fat_path.py
```

The directory suite passes **7,187 scenarios / 21,561 executions** and the
path suite **758 / 2,274** across original x86, ordinary C, and independently
linked fixed-address C. Existing ten directory and two file-path/name-format
entries remain in these suites. Complete checked memory, return values,
ordered calls and cdecl stack/callee-saved registers agree.

The directory suite uses all 12 original directories and 101 occupied records.
New checks include all 256 attributes, duplicate displayed names, live root
bounds/geometry/attributes, cached child bounds, wrapping and unaligned
date/time reads, deleted/end/LFN records, and unsigned 16-bit edge values. No
directory/name/comparison service is stubbed. Hooks only observe calls and
apply explicit comparison-time memory mutations.

The path suite checks all 11 original child directory paths with case and
trailing-slash variants, and rejects all 67 file paths as directories. It also
covers malformed/drive-only inputs, all attribute bytes, deep paths/parser
caps, signed high-bit names, long-extension aliases, allocation failure and
free-before-copy behavior. Only allocator/free and hardware-sector boundaries
are modeled; pathname, directory walk and FAT helpers execute actual x86.

Arbitrary invalid pointers, unbounded destructive aliases and cyclic FAT
chains are outside these fixtures. Preview reports are in
[`../evidence/2026-10-29-fat-directory-path/`](../evidence/2026-10-29-fat-directory-path/).
This source checkpoint precedes production installation. Kernel/INIT remain
partial; no new application is classified as rebuilt.

## Integrated validation

All seven bodies are installed at the original addresses; the kernel now has
**385 declared intervals**. Independent preview and production-linked bytes
agree. Installed original/ordinary-C/fixed-C comparisons pass 7,187 directory
scenarios / 21,561 executions, 758 path scenarios / 2,274 executions, 2,410
reader scenarios / 7,230 executions, and 430 X-loader scenarios / 1,290
executions. Icon-directory, icon-file and bitmap-file caller regressions,
exact whole-disk kernel/application checks, six branding tests, residual audit
and inventory validation pass.

```sh
make -C rewrite build/OpenIceStarOS_c_apps.vhd \
  build/OpenIceStarOS_fat_diagnostic.vhd \
  test-fat-directory test-fat-path test-fat-files test-x-loader
make -C rewrite inventory test-branding \
  test-icon-directory test-icon-files test-bitmap-files
make -C rewrite inventory-check
python3 rewrite/tools/boot_smoke.py \
  rewrite/build/OpenIceStarOS_fat_diagnostic.vhd \
  --fat-relative-check --memory-check --task-check
python3 rewrite/tools/boot_smoke.py \
  rewrite/build/OpenIceStarOS_c_apps.vhd \
  --clock-check --fat-files-check --memory-check --task-check
```

The separate diagnostic X calls 25 production service addresses: eight readers
and seventeen directory entries. Its 37 checked calls verify **526,144
data/guard bytes**, results, relocations and allocation/free paths using actual
kernel services and disk I/O. A copied root buffer independently exercises
supplied-directory entries. Both relative directory finders return index 2 for
`ICESTAR`, bypassing the identically named volume label. Absolute-directory
calls check `C:\ICESTAR\GAMES`, `C:\ICESTAR`, a missing child, drive-only input
and a doubled separator. All 32 bytes of both successful output records and
the surrounding 16-byte guards agree with independent FAT reads; error cases
leave output/guards intact. All four timestamp getters agree with the raw
INIT record. QEMU observes every new entry, plus interior blocks for all three
lookups. The small getters each execute in one translation block. This
diagnostic replaces COLOR.X only in its separate image and is not an app
recovery.

Ordinary production boot additionally observes one root directory-only lookup
and two absolute-directory lookups. The supplied-buffer finder and four
timestamp entries have no observed calls in this production workload; they
are covered by the diagnostic and differential suites. Branded legacy CMD
and `cpuid`, C CLOCK with three complete changing frames, PS/2 close, task exit,
CMD reuse, 128 MiB RAM/heap and real TSS switches pass. Both snapshot boots
verify all 385 installed intervals against guest memory. The committed CLOCK
and CMD screenshots are direct QEMU captures and were visually inspected.

Exactly **1,630 bytes** differ from the preceding 378-interval production
image, all inside the seven new slots. Restoring the preceding kernel
reproduces that whole image byte for byte. All non-kernel disk bytes, including
the original startup logo, FATs, directories, slack and apps remain unchanged;
the original VHD hash is unchanged. `image-delta.json` records per-slot changes
and image/kernel hashes.

Committed reports and screenshots are in
[`../evidence/2026-10-29-fat-directory-path/`](../evidence/2026-10-29-fat-directory-path/).
This was focused integration validation; a full 385-interval `make verify`
was not run. The interval count is not a completion percentage. Five apps are
rebuilt; INIT/kernel are partial and 32 program paths remain unrecovered.
The build still uses original program logic and resources, and remaining FAT
mutators, hardware I/O, drivers and applications still need recovery.
