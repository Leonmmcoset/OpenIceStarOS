# OpenIceStarOS display installation

`patches.json` is a display-only installation manifest, separate from the C
recovery manifests. Both CMD paths and TASKBAR remain **unrecovered**. It never
registers them as rebuilt applications or adds kernel recovery intervals.

Each original file is SHA-256 checked. Edits specify exact old bytes and
equal-length replacement bytes. The FAT installer preserves directory entries,
both FATs, file sizes, X headers, relocation tables and allocation slack. Kernel
branding is applied after C interval installation and rejects overlap with any
declared source interval. Whole-disk verification reconstructs both the compiled
code and the declared branding bytes; it still rejects every other difference.

- CMD's original GBK version label is shortened to fit its existing slot:
  `OpenIceStarOS [%d.%d.%d]`. The three integer arguments and NUL termination
  remain unchanged; both relocated references retain their original offsets.
- The command list uses `OpenIceStarOS Commands:` in its original slot.
- TASKBAR uses the equal-length `Welcome OpenIceStarOS`.
- The kernel startup slot at `0x83cce3` contains `OpenIceStarOS`. The original
  fatal-dialog title pointer at file `0x137fe` (`mov [esp+8], 0x83cdc4`)
  points to that slot, and its former short string is cleared. This is an
  explicit display operand edit in original code, not recovered C logic.
- README.TXT uses GBK text identifying the incomplete C recovery. Its file
  length stays 68 bytes, with trailing spaces.

`make test-branding` checks full-disk restoration, X metadata, CMD relocation
at two load bases, formatting, neighboring kernel strings, source-hash
rejection, undeclared disk mutation rejection and live-file residuals.
`tools/branding.py scan IMAGE REPORT` scans boot sectors and every live FAT file
for ASCII/GBK-embedded Latin names and UTF-16 LE/BE variants. It reports retained
compatibility paths. The raw disk additionally retains two old display strings
in the kernel's final-cluster slack and eight in four free clusters. These bytes
are outside all live file lengths and stay unchanged as original disk evidence.
The scanner verifies that classification instead of treating them as live UI.

The original VHD, extracted files, `reverse/`, original differential kernel
`build/icestar.x`, FAT label/directory `ICESTAR`, file `ICESTAR.X`, API/ABI and
`C:\ICESTAR\...` paths retain their original identities. The root Makefile is
the original byte-exact assembly reference, including its old menu. Current
OpenIceStarOS builds use `rewrite/Makefile`.

Actual QEMU menu, desktop, branded CMD with `cpuid`, and rebuilt CLOCK captures
are committed in `../evidence/2026-10-28-branding/`. PNGs are direct conversions
of monitor screendumps, with no image editing. The desktop wallpaper is an
unchanged inherited resource. Runtime checks passed for 128 MiB RAM, 59 heap
headers, real TSS task switching, PS/2 CMD/cursor behavior, all 352 installed
kernel intervals, changing CLOCK frames, window close, task exit and CMD reuse.

This closes the display-branding checkpoint. It does not complete OS recovery;
the full regression is recorded separately when it finishes.
