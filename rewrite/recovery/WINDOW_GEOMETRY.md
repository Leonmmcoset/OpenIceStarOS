# Window maximize, global layout and restore recovery

Three real bodies now execute `kernel/window_geometry.c`, taking the installed
kernel from 611 to 614 intervals. With the preceding focus batch this turn adds
seven actual functions. These counts do not measure OS completion.

Maximize requires a positive signed style, mode 3 and state zero. It compares
the signed, wrapped backing area with screen area, frees and reallocates the
backing only when smaller, and preserves the original unchecked allocation
result. Bytes per pixel use signed division by eight, truncated toward zero.
The selected surface pointer is captured before allocation; owner and area are
read in the original order. Logical position/size are saved in object
`+0x12b..+0x137`. Modes 0/5 use the full screen, 1 reserves a bottom taskbar,
2 a left taskbar, 3 a right taskbar and 4 a top taskbar. Other layout values
leave logical geometry unchanged. Backing dimensions are still updated.

The original client width subtracts the left offset twice; height subtracts
top and left offsets. Title width is logical width minus six. Maximize sets
state 1/mode 4, draws with initialize=1, calls the live `+0x573` callback, draws
again with initialize=0 and submits the live surface rectangle. No extra
validation, rollback or overflow correction is introduced.

Global layout walks the live object count and skips nonpositive signed table
pointers. State-one objects update backing/layout/client geometry, draw once
and call their callback. A subsequent separate state-zero check calls the
callback again if it changed state to zero; undecorated state-zero objects also
receive callbacks. Saved pre-maximize geometry is not overwritten. Restore
requires positive style, mode 4/state 1, reapplies the saved logical geometry,
sets mode 3/state 0, draws/calls back/draws, then refreshes the appropriate
desktop region from surface 2's backing dimensions. The backing remains large.

`make -C rewrite test-window-geometry` passes **440 scenarios / 1,320 executions**
against original x86, ordinary C and installed C: 164 maximize, 124 global-layout
and 152 restore cases. It executes real owner allocation/free and actual x86
callbacks. Frame drawing and compositor submissions are explicit native
boundaries, compared by ordered arguments and live mutations. Independent
oracles check six taskbar layouts, saved/client/title geometry, flags/state and
counter effects. Tests include signed area overflow, negative pixel depth,
allocation failure, unchanged/larger backing, invalid state/style gates,
wrapped IDs, live object count, negative table pointers, callback argument-copy
isolation and the duplicate-callback behavior. All touched heap/data pages,
caller arguments, cdecl registers and x87 control compare.

`test-window-geometry-qemu` runs real original/recovered heap growth, frame
drawing, compositor and callbacks through CMD/PS2. A real 120x80 window with a
small backing is configured for maximize. The diagnostic temporarily fixes
taskbar dimensions at 32x28 and exercises all six orientations on 1024x768,
checking exact dimensions, saved restore geometry, expanded backing and three
callback observations per cycle. The layout walk temporarily borrows slot zero
and count=1 to isolate this window from other applications' resize callbacks;
the original taskbar callback would otherwise rewrite the fixture dimensions.
Those table entries, taskbar data and CPU flags are restored before the normal
31-service lifecycle diagnostic. No heap/drawing/compositor function is mocked.
All three new bodies execute, both independent **108-DWORD reports** match and
both complete 33,554,944-byte disposable disks remain unchanged. Temporary
diagnostic programs and guest fixtures are not product recovery entries.

On 614 intervals, the 1,208 lifecycle, 2,270 window-service and 816 image-control
scenario regressions pass. Production CLOCK verifies all installed slots,
RAM/heap, timer/TSS switching, PS/2/CMD/cpuid, cursor, clock face/labels, three
changing frames, close/task exit and CMD reuse. Kernel allowed-range and
application whole-disk verification and inventory checks pass. Actual viewed
[CLOCK capture](../evidence/2026-11-03-window-geometry/clock.png), summaries,
logs and hashes are in [the evidence directory](../evidence/2026-11-03-window-geometry/README.md).

The last complete `make verify` is still 601 intervals; 614 has these focused
and neighboring checks. Arbitrary corrupt state, every display mode and all
concurrent callback interleavings are not covered. INIT/kernel remain partial,
five apps are rebuilt and 32 paths, including four drivers, remain original.
The complete OS rewrite is unfinished. Original VHD and logo are retained.

| Entry | Address | Compiled / slot bytes |
| --- | --- | --- |
| window_maximize | `0x80ed56` | 1256 / 1896 |
| window_layout_all | `0x80f4be` | 1048 / 1620 |
| window_unmaximize | `0x80fbfc` | 732 / 1074 |
