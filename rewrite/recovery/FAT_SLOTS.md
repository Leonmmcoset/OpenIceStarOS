# FAT16 free-slot and cluster scans

This checkpoint recovers four original-only FAT helpers in C. The routines
retain the original unchecked arithmetic and live memory reads; they are
compatibility recoveries rather than hardened filesystem code.

| Address | Operation | Original slot bytes | Compiled C bytes |
| --- | --- | ---: | ---: |
| `0x82798c` | Scan the root directory for an unused or deleted entry | 180 | 117 |
| `0x827a40` | Scan a supplied directory buffer for an unused or deleted entry | 142 | 70 |
| `0x827ace` | Find the first zero FAT16 entry, beginning at cluster 2 | 118 | 68 |
| `0x827b44` | Find the first zero FAT16 entry after a supplied cluster | 118 | 66 |

The two directory scans use the BPB root-entry count as their bound. Each
performs two complete passes: byte `0` wins first, then byte `0xe5` marks the
first deleted record. A full scan returns `-1`. The supplied-directory scan
uses the same bound as the root scan, exactly as the original does.

The cluster scans read the cached FAT pointer at `0x1a83a`. Their limit is
the signed, truncation-toward-zero half of the 32-bit product of FAT sectors
and bytes per sector. The first scan starts at cluster 2; the second starts at
`cluster + 1`. A zero 16-bit FAT entry is returned, and exhaustion returns
`-1`. Product and index arithmetic wraps at 32 bits before the original signed
comparisons.

## Validation

`tests/kernel_fat_slots.py` executes the original x86 bodies, ordinary linked
C, and fixed-address C for 208 scenarios and 624 total executions. It checks
both directory-pass ordering, empty/full boundaries, deleted records, live
BPB geometry, FAT pointer changes, signed product limits, wrapping starting
clusters, return values, stack cleanup, and callee-saved registers. The
installed test also compares every compiled byte in all four production slots
against `openicestaros_kernel.x`.

```sh
make -C rewrite build/openicestaros_kernel.x test-fat-slots
make -C rewrite inventory-check
```

The kernel now declares 390 replacement intervals. The four helpers are used
by the original directory/file mutation paths; the production image keeps
those callers at their original addresses while replacing only these four
intervals. INIT, drivers, and most applications remain original or partial;
this focused recovery does not complete the operating system rewrite.
