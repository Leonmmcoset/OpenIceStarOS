# DEFLATE and zlib recovery

Ten additional original entries at `0x8380ef..0x8393a1` now execute
`kernel/inflate.c`, increasing installed intervals from 540 to 550. Together
with the preceding23 PNG entries, all33 function entries in the contiguous
9,760-byte PNG/DEFLATE code range `0x8380ef..0x83a70f` are C implementations.
The original fixed-Huffman trees, length/distance tables and code-order data
are still retained as read-only data at their original addresses; they are not
additional restored functions. Other kernel logic, INIT, drivers and apps
remain partially/unrecovered. This is not a complete OS rewrite.

| Original entry | Address | Compiled / original slot bytes |
| --- | --- | --- |
| png_bit | `0x8380ef` | 47 / 52 |
| png_bits | `0x838123` | 49 / 74 |
| png_tree_init | `0x83816d` | 31 / 32 |
| png_tree_build | `0x83818d` | 509 / 908 |
| png_symbol | `0x838519` | 142 / 198 |
| png_dynamic | `0x8385df` | 813 / 1508 |
| png_compressed | `0x838bc3` | 650 / 1004 |
| png_stored | `0x838faf` | 258 / 318 |
| png_inflate | `0x8390ed` | 273 / 374 |
| png_zlib | `0x839263` | 197 / 318 |

## Preserved behavior

Bit input reads least-significant-first and advances the caller's DWORD bit
position; multi-bit shifts retain x86's modulo32 shift count. Tree descriptors
are exactly nodes/maxbits/symbol-count, with original store order. Canonical
tree construction computes symbol codes, inserts MSB-first paths, reports
overfilled nodes using error4/source0xaf, and replaces unused branches with
symbol0. The original symbol reader checks the byte boundary using **>**, not
>=, before consuming bits; invalid next-node values report0xde. Its bounded
input check is not a complete memory-safety guarantee.

Dynamic blocks read HLIT/HDIST/HCLEN, fill the code alphabet in original RFC
permutation order, expand repeated previous lengths16 and zero lengths17/18,
require an EOB length and build the literal/distance trees. Error codes,
original source-line numbers and partial writes remain. Fixed blocks use the
original read-only trees. Literal writes allow exactly filling the output;
length/distance copies and stored blocks reject `written+length >=capacity`.
Distance copies cycle over the already decoded source span. Distances30/31
are rejected, but distance-before-output is unchecked as in the original.
Stored blocks align the bit cursor, validate LEN+NLEN and input/output bounds,
then copy forward with live write-position reads. Their header check also
rejects a zero-length raw block with exactly five bytes and no trailing byte.

The raw block dispatcher preserves final/type bits and reports forbidden type3.
The zlib wrapper checks FCHECK, CM/CINFO and FDICT, then starts at input offset2
while continuing to pass the original whole input size; it does not validate
Adler-32. Existing object errors are propagated at the same checkpoints.
`count[15]` in tree construction is initialized in C to avoid evaluating an
indeterminate value: the original increments this uninitialized stack word,
but only counts0..14 feed its code recurrence, so it has no result effect.

## Validation

`make -C rewrite test-inflate` passes **3,624 scenarios / 10,872 executions**
against original x86, ordinary C and installed C. Independent canonical-tree
oracles check exact nodes and symbol codes. Input bit offsets0..63, bit counts
through63 including shifts beyond31, incomplete and oversubscribed bounded
trees, invalid nodes, stored blocks with alignment/truncation/size/complement
cases, repeated source spans, output capacity boundaries and zlib header
errors are covered. Direct dynamic fixtures include all three length-repeat
symbols16/17/18 and independent EOB/distance tables. Generated compressed
payloads are independently checked by Python zlib and their output bytes are
then checked directly; no nested helpers are mocked. Complete touched pages,
caller words, cdecl registers/stack and x87 state are compared.

`make -C rewrite test-png` also passes **3,746 scenarios / 11,238 executions**
with the complete installed C chain. Its low-depth output checks compare
meaningful pixel bits independently and compare every heap byte against the
original, including unspecified trailing bits. Native full-chain totals for
the two suites are **7,370 scenarios / 22,110 executions**.

`test-inflate-qemu` starts the same diagnostic X through real CMD/PS2 in
original/recovered disposable images. Hand-encoded stored/fixed/dynamic
streams produce independent byte hashes, exact-full stored-output failure,
bit-cursor reports and tree nodes. The dynamic guest stream exercises length
repeats16/17/18. QEMU requires all ten entries and decoded function bodies.
`test-png-qemu` is rerun with all33 installed entries; all23 PNG entry/body
traces and its pixel/filter/lifecycle report match. All source/disposable disks
are compared completely and remain unchanged. Diagnostics are not rebuilt apps.

Production COLOR boot verifies all550 loaded intervals, 128MiB RAM, heap,
real TSS/timer switching, PS/2 mouse/CMD input/cpuid/cursor, 600 palette pixels,
window close, task exit and CMD reuse. Real screenshots and summaries are in
`../evidence/2026-11-01-inflate/`. Entire kernel/application disk validation and
inventory checks pass. The original VHD hash remains unchanged. Focused
font-bake and X-loader regressions also passed during this integration.

Full `make verify` has not been rerun; its last complete recorded result is
456 intervals. Arbitrary invalid pointers, caller/stack alias attacks, oversized
code lengths/counts, early repeat16 without a prior length, dynamic errors
followed by use of uninitialized local trees, and wrapped/cyclic unbounded
malformed streams are outside the validated scope. Original unchecked parser
behavior is not claimed safe. Five apps are rebuilt;32 program paths remain
unrecovered. The complete operating-system rewrite remains unfinished.
