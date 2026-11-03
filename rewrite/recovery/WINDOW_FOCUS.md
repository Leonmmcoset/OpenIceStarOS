# Window stacking, focus and title-button recovery

Four bodies now execute `kernel/window_focus.c`, taking the kernel from 607
to 611 installed intervals. These are actual window-manager functions, not
new applications or a completion percentage.

Raise appends an absent surface, preserves taskbar/pinned/drag placement,
orders relative to the taskbar or cursor, refreshes its rectangle and clears
the hidden flag only when that flag equals one. Activate additionally changes
the active object ID, sets the packed active bit, redraws supported captions
and signals notification word zero. A repeated activation only refreshes;
surface IDs 2/3 take the original early return after activation/append.
Surface order comparisons are unsigned; style checks are signed.

Deactivate captures the object, parent and current object before checking the
active bit. It clears that bit, conditionally redraws its own and its parent's
caption, submits the parent title damage, calls the live `+0x5a7` callback,
refreshes the object and signals notification word one. Notification pointers,
surface fields and callback fields are read in the original sequence after
external calls. No null-parent validation, flag normalization or incidental
EAX promise is added to these void APIs.

The fourth function selects title buttons, rather than the client or resize
border. It reads the selected theme table without adding a style-index guard.
Themes 1/2/3 use one set of rectangles; themes 4/5 shift the horizontal edges
three pixels and enlarge the vertical range. Mode 1 returns close code 1;
mode 2 also returns code 4; higher signed modes return codes 1/2/3. All four
rectangle edges are excluded. Coordinate addition wraps as a DWORD before
signed comparisons. Unsupported themes/modes return zero.

`make -C rewrite test-window-focus` passes **39,174 scenarios / 117,522
executions** against original x86, ordinary C and installed C. These include
333 raise, 333 activate, 364 deactivate and 38,144 button-hit cases. Independent
expectations cover ordered stack/caption/refresh calls, flags and notification
fields, strict button edges, themes, modes, negative/wrapped coordinates,
reserved surfaces, parent/current relationships, live boundary mutations and
an actual x86 callback modifying its private argument. Native surface-order,
caption and refresh boundaries are explicit models; they are not recovered
components. All touched data, caller arguments, cdecl registers and x87 controls
are compared. A void return's incidental EAX is intentionally excluded.

`test-window-focus-qemu` runs the original and rebuilt kernels through actual
CMD/PS2 with real allocation, surface ordering, caption painting and compositor.
Two windows check focus gain/loss, callback identity/count, notification words,
raise-without-activation and repeated activation. A temporary theme table
fixture checks five theme classes and exact button return codes, then restores
the theme before closing. The existing 31-service diagnostic runs afterward.
Both independent **86-DWORD reports** agree, all four new entry bodies execute,
and both full 33,554,944-byte disposable disks remain unchanged. The temporary
diagnostic X is not a rebuilt application and is not installed in the product.

The 1,208-scenario window-lifecycle and 2,270-scenario window-service regressions
pass on the installed 611-interval kernel. Production COLOR verifies all slots
against guest memory, RAM/heap, timer/TSS switches, PS/2/CMD/cpuid, cursor,
600 palette pixels, close/task exit and CMD reuse. Kernel allowed-range and
application whole-disk checks and inventory consistency pass. Actual viewed
[COLOR screenshot](../evidence/2026-11-03-window-focus/color.png), logs, hashes
and summaries are in [the evidence directory](../evidence/2026-11-03-window-focus/README.md).

The last complete `make verify` remains the 601-interval run; 611 has these
focused and neighboring checks. Arbitrary invalid pointers, all display modes
and all concurrent interleavings are outside coverage. INIT/kernel remain
partial, five applications are rebuilt, and 32 paths including four drivers
remain original. The complete OS rewrite is unfinished. Original VHD and logo
are unchanged.

| Entry | Address | Compiled / slot bytes |
| --- | --- | --- |
| window_raise | `0x8084a0` | 605 / 832 |
| window_activate | `0x8087e0` | 679 / 920 |
| window_deactivate | `0x808b78` | 434 / 576 |
| window_button_hit | `0x808db8` | 1213 / 2406 |
