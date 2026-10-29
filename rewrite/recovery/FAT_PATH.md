# FAT16 absolute pathname and short-name recovery

`kernel/fat_path.c` recovers two previously original-only functions:

| Address | Operation | Original slot bytes | Compiled C bytes |
| --- | --- | ---: | ---: |
| `0x8263c0` | Parse absolute path, walk directories, copy file record | 1,408 | 606 |
| `0x827bba` | Convert final path component into an uppercase FAT 8.3 name | 184 | 135 |

The lookup parses up to 32 components into 13-byte slots. A thirteenth
non-separator character rejects a component; empty backslash-separated
components reject the path, while one trailing backslash is accepted. Each
completed component is uppercased. The first component need only **start with
`C:`**, so `C:EXTRA` retains the original acceptance behavior. Components beyond
the parser's 32-slot limit are ignored rather than causing a new rejection.

The root comes from the loaded BPB/root directory at `0x520000`. Intermediate
directories are found by their eleven short-name bytes and directory bit;
the final file rejects volume/directory attributes. Zero entries terminate
scans and deleted entries are skipped. Child-directory scan bounds use one
cached cluster's byte count divided by 32, even when a read follows a longer
FAT chain. The complete 32-byte final directory record is copied to the
caller. Return values remain -1 for parser rejection, 0 for not found or an
unrecognized drive prefix, and 1 for a matching file. Allocation failures are
not given new handling.

The original compares a signed short-name byte with an unsigned directory
byte, rejecting high-bit name bytes. Its formatter uses the last backslash,
uppercases each byte, pads the basename to eight positions when it sees a
dot, and always places a NUL at output+11. It does not implement a stricter
8.3 validator: extra dots/extensions can map to an existing short name, and
long extensions can write beyond eleven output bytes. The adjacent local
short-name/path arrays retain their original layout for these bounded parser
inputs; no new normalization or truncation policy is introduced.

Directory-buffer lifetime also retains the original behavior: a single loaded
directory is not freed on the ordinary final lookup path, while deeper walks
free the previous buffer **before** reading the selected record from it. The
free threshold and ordering are deliberately preserved, including failure
cleanup. This recovery does not independently repair those original behaviors.

## Source validation

```sh
cd rewrite
python3 tests/kernel_fat_path.py
```

The preview passes **347 scenarios / 1,041 original x86, ordinary C and
fixed-address C executions**. Only allocator/free and hardware sector-read
boundaries are intercepted. Parsing, short-name conversion, uppercase helpers,
directory scanning, FAT traversal and output record copies execute as x86;
no pathname/directory callback supplies successful lookup results.

All **67 original files**, including all 39 program paths and resource files,
are found through their real on-disk directories in uppercase, lowercase and
trailing-backslash forms. Returned records are independently checked against
FAT extraction. Synthetic tests cover malformed prefixes/separators, component
and depth limits, 8.3 aliases, signed bytes, attributes/deletions/end markers,
allocation failures, the original lifetime thresholds, and a free-boundary
mutation of the record that must be consumed after free. Directory allocation
and free counts have independent expectations. Format tests include long
extensions, repeated dots and an in-place non-dot name. Return values, cdecl
preservation/caller stack, ordered allocation/free/sector arguments and all
checked low-memory, BPB/FAT, output and directory-arena bytes are compared.

Unbounded FAT cycles, arbitrary unmapped pointers, unbounded destructive
input/output aliases and original stack-overflow inputs outside the parser's
bounded cases are excluded. Preview evidence is in `build/fat_path_summary.json`
and `build/fat_path_preview.log`. This checkpoint precedes production installation;
INIT, the kernel, drivers and other unrecovered programs remain incomplete.

## Integrated validation

Both C bodies are installed at the original exported addresses in the
production kernel, increasing the declared intervals from 364 to 366. Their
installed bytes agree with an independent preview link. The installed path
test passes all 347 scenarios / 1,041 executions while using the production
C string and FAT-reader dependencies; the ordinary link still uses original
helper bodies. The existing twelve-reader FAT suite passes 1,926 scenarios /
5,778 executions, and the X-loader suite passes 430 / 1,290. Installed icon-file
and bitmap-file differential checks, exact kernel/application disk comparison,
six branding tests, residual audit and inventory validation also pass.

```sh
make -C rewrite build/OpenIceStarOS_c_apps.vhd \
  test-fat-path test-fat-files test-x-loader
python3 rewrite/tools/boot_smoke.py \
  rewrite/build/OpenIceStarOS_c_apps.vhd \
  --clock-check --fat-files-check --memory-check --task-check
python3 rewrite/tools/boot_smoke.py \
  rewrite/build/OpenIceStarOS_c_apps.vhd \
  --cursor-check --memory-check --task-check
```

The ordinary production QEMU boot executes the new C path entry **47 times**
and short-name entry **119 times**, including C-body blocks for both. No
diagnostic application or pathname callback is used for this execution proof.
The desktop, CMD/cpuid, changing CLOCK frames, close/task exit/CMD reuse,
128 MiB RAM/heap, real TSS switching and all 366 installed guest intervals pass.
The second production boot checks unchanged original DEVMGR.X through CMD,
its device list, border cursor behavior and PS/2 close. Both boots use snapshots.

An exact comparison with the previous 364-interval production image finds
**1,542 changed bytes**, all inside the two newly recovered kernel slots.
Boot stages, all application files, FAT/directory structures, disk slack and
the original retained logo/resources are byte-identical. The original VHD's
hash also remains unchanged. Evidence and inspected screenshots are in
[`../evidence/2026-10-29-fat-path/`](../evidence/2026-10-29-fat-path/).

This is a focused 366-interval integration checkpoint, not a full `make verify`
regression. Kernel and INIT are still partial, five applications are rebuilt
and 32 program paths remain unrecovered. Other lookup variants, directory
name extraction, remaining filesystem routines and hardware I/O still require
source recovery; no unchanged binary is reclassified as rebuilt here.
