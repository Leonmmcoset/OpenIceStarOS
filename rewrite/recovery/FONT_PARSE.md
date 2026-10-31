# CFF/SFNT parser foundations

Twenty-four functions at 0x82b838..0x82c3bc execute C from
`kernel/font_parse.c`. All entries retain original addresses and ABI. The
kernel now installs 430 intervals, including 77 exact instruction intervals;
these counts do not indicate semantic completion percentages.

## Recovered functions

| Function | Compiled / slot bytes |
| --- | --- |
| `font_get8` | 39 / 54 |
| `font_peek8` | 32 / 52 |
| `font_seek` | 28 / 60 |
| `font_skip` | 23 / 32 |
| `font_getn` | 43 / 72 |
| `font_buffer` | 28 / 28 |
| `font_range` | 87 / 104 |
| `font_cff_index` | 101 / 166 |
| `font_cff_int` | 141 / 256 |
| `font_cff_skip` | 75 / 116 |
| `font_cff_dict` | 122 / 228 |
| `font_cff_ints` | 67 / 96 |
| `font_cff_count` | 28 / 46 |
| `font_cff_item` | 107 / 190 |
| `font_u16` | 22 / 32 |
| `font_s16` | 20 / 30 |
| `font_u32` | 39 / 52 |
| `font_s32` | 39 / 52 |
| `font_signature` | 186 / 300 |
| `font_table` | 113 / 204 |
| `font_offset` | 129 / 228 |
| `font_count` | 109 / 156 |
| `font_cff_subrs` | 162 / 282 |
| `font_svg` | 80 / 112 |

The functions implement byte stream access/seek/skip/big-endian accumulation,
CFF buffer ranges, INDEX extraction/item access, DICT integer decoding and
real-operand skipping, DICT lookup/integer arrays, local Subrs lookup,
big-endian signed/unsigned SFNT access, font signatures, TTC offsets/counts,
SFNT table lookup and lazy SVG table discovery.

## Compatibility details

Buffers hold a 32-bit address, signed cursor and signed size. Reads compare
cursor/size as signed integers; a negative cursor is not rejected. Exhausted
reads return zero without advancing. Seek clamps negative/out-of-range
positions to size, skip wraps at 32 bits and GETN accumulates modulo 32 bits.
CFF widths, offsets and sizes retain original unchecked behavior. In
particular, DICT short integers are not sign-extended, real operands are
skipped at either terminating nibble and escape operators use bit 0x100.
Missing dictionary operands remain untouched in the caller's output array.

The 12-byte structure results use the original i386 hidden output pointer
and `ret 4` convention. The range entry writes its zero result before
reading input bounds, which matters when buffers overlap. INDEX item and
local Subrs helpers advance by-value buffers in their caller argument area.
These effects are tested, including the precise hidden-pointer stack pop.
The range helper explicitly accesses the hidden output pointer through the
GCC i386 frame layout; this is architecture/compiler specific and validated
against the production bytes, not claimed to be portable C.

Font recognition retains all five original signatures, including `1\0\0\0`.
TTC versions 1/2 use signed counts and permit negative indexes below count;
no new index validation is added. Table tags compare unsigned font bytes
against signed query bytes, so tags containing high bytes do not match.
The SVG lookup retains signed-negative cache invalidation and live info
reads. The original shipped font remains a reference resource, not newly
written source or a newly licensed asset.

## Validation

`tests/kernel_font_parse.py --installed` compares original x86, ordinary C
and installed C in 5,371 scenarios / 16,113 executions, without intercepting
any helper. It checks returned values and buffers, all touched data pages,
callee-saved registers and the caller's argument area, including intentional
by-value mutations. Undefined volatile-register returns of void entries and
local stack scratch are excluded. Cases cover signed cursor/count bounds,
overlapping output/input descriptors, every CFF initial byte, truncated and
malformed INDEX widths, integer boundaries, real terminators, escaped and
repeated DICT operators, private dictionaries, SVG presence/cache states,
SFNT/TTC signatures, byte order, table-offset wrapping and deterministic
random samples. Every table in the original Fira font is independently
checked against its header directory.

`tests/font_parse_guest.c` is a test-only X substituted for COLOR.X on
separate disposable images. Original and recovered kernels launch it via
real CMD/PS2 input. All 24 entries and the original full font initializer
execute in the guest, with real allocation and disk reads. CFF fixtures
check aggregate results/cursors and integer values; the shipped Fira font
loads and initializes successfully. A host-parsed table directory specifies
the expected glyph count, table offsets and success results. Both reports
match that independent expectation. Each complete 33,554,944-byte disk
remains identical to its seeded image, and source images are unchanged.

The shared QEMU runner optionally logs decoded instructions as well as
executed blocks. Some short leaf functions occupy a single entry block;
entry execution, its decoded instructions and the independently checked
report establish their body execution. Existing callers retain their
previous block-only checks by default.

Focused regressions pass contiguous/file/relative FAT readers, paths,
directory accessors, writers, release/deletion, metadata, free slots and X
loading. Kernel/app installers and inventory checks pass. Production CLOCK
load/animation/close/task exit/CMD reuse, CMD/cpuid, 128 MiB RAM with 59 live
heap headers, timer/TSS switching and all 430 installed guest intervals pass.

    make -C rewrite test-font-parse test-font-parse-qemu
    make -C rewrite test-fat-block test-fat-files test-fat-path \
      test-fat-directory test-fat-write test-fat-mutation test-fat-metadata \
      test-fat-io test-fat-slots test-x-loader
    python3 rewrite/tools/boot_smoke.py rewrite/build/OpenIceStarOS_c_apps.vhd \
      --clock-check --memory-check --task-check

Small JSON evidence is stored in `../evidence/2026-10-31-font-parse/`.
Build images, traces, memory dumps and screenshots remain ignored. Full
`make verify` was not run. Tests do not establish safety for arbitrary
invalid font pointers, unbounded counts or all malformed inputs. Font
outlines, rasterization, the complete initializer and other kernel code
remain original. INIT/kernel are partial, five applications are rebuilt,
and 32 program paths remain unrecovered. Complete OS recovery is unfinished.
