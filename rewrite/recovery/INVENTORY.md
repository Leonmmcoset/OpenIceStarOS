# Binary recovery inventory

Generated from the original VHD and the committed recovery manifests by
`python3 tools/binary_inventory.py`. Recheck with `make inventory-check`.

There are 39 original `.X`/`.DX` paths and 37 distinct binaries. 5 applications are rebuilt, 2 programs are partially recovered and 32 paths still use original programs.

The complete OS recovery is unfinished. Rebuilt applications continue to
depend on original guest components. Kernel interval/byte counts describe
replacement slots, including padding and hardware instructions, and are
not completion percentages. Detailed original SHA-256 values and raw X
headers are in [inventory.json](inventory.json).

| Original path | Bytes | Role | Recovery |
| --- | ---: | --- | --- |
| `/ADMIN/DESKTOP/CMD.X` | 65,492 | application | unrecovered |
| `/ADMIN/DESKTOP/DEVTEST.X` | 34,586 | application | unrecovered |
| `/ADMIN/DESKTOP/NETWORK.X` | 48,418 | application | unrecovered |
| `/ADMIN/DESKTOP/TASKMGR.X` | 42,190 | application | unrecovered |
| `/ICESTAR/AREOBAR.X` | 48,950 | application | unrecovered |
| `/ICESTAR/BROWSER.X` | 17,304 | application | unrecovered |
| `/ICESTAR/CALC.X` | 45,274 | application | unrecovered |
| `/ICESTAR/CHAT.X` | 35,952 | application | unrecovered |
| `/ICESTAR/CLOCK.X` | 15,576 | application | rebuilt |
| `/ICESTAR/CMD.X` | 65,492 | application | unrecovered |
| `/ICESTAR/COLOR.X` | 14,516 | application | rebuilt |
| `/ICESTAR/DESKTOP.X` | 54,966 | application | unrecovered |
| `/ICESTAR/DEVMGR.X` | 33,482 | application | unrecovered |
| `/ICESTAR/DRIVERS/AC97.DX` | 17,782 | driver | unrecovered |
| `/ICESTAR/DRIVERS/E1000.DX` | 38,680 | driver | unrecovered |
| `/ICESTAR/DRIVERS/HDA.DX` | 25,538 | driver | unrecovered |
| `/ICESTAR/DRIVERS/SB16.DX` | 15,966 | driver | unrecovered |
| `/ICESTAR/GAMES/ELSFK.X` | 80,694 | application | unrecovered |
| `/ICESTAR/GAMES/GLASS.X` | 85,588 | application | unrecovered |
| `/ICESTAR/GAMES/INVADER.X` | 33,390 | application | unrecovered |
| `/ICESTAR/GAMES/MINE.X` | 69,330 | application | unrecovered |
| `/ICESTAR/GAMES/MOV/MOV.X` | 14,932 | application | rebuilt |
| `/ICESTAR/GAMES/NES/NES.X` | 57,680 | application | unrecovered |
| `/ICESTAR/GAMES/PUSHBOX.X` | 68,046 | application | unrecovered |
| `/ICESTAR/GAMES/SNAKE.X` | 2,852 | application | rebuilt |
| `/ICESTAR/ICESTAR.X` | 250,099 | kernel | partial; 320 declared intervals |
| `/ICESTAR/MACBAR.X` | 27,758 | application | unrecovered |
| `/ICESTAR/MPLAY.X` | 31,564 | application | unrecovered |
| `/ICESTAR/NOTEPAD.X` | 62,326 | application | unrecovered |
| `/ICESTAR/PAINT.X` | 75,490 | application | unrecovered |
| `/ICESTAR/PCMGR.X` | 45,914 | application | unrecovered |
| `/ICESTAR/PICVIEW.X` | 43,090 | application | unrecovered |
| `/ICESTAR/SCRNSET.X` | 35,516 | application | unrecovered |
| `/ICESTAR/STARSCR.X` | 16,052 | application | rebuilt |
| `/ICESTAR/TASKBAR.X` | 53,446 | application | unrecovered |
| `/ICESTAR/TASKMGR.X` | 42,190 | application | unrecovered |
| `/ICESTAR/THEME.X` | 39,038 | application | unrecovered |
| `/ICESTAR/TTFVIEW.X` | 45,506 | application | unrecovered |
| `/INIT.X` | 26,817 | init | partial |

Boot regions are separate from the program-file count:

- MBR at `0x0`, 512 original bytes: C implementation with assembly entry and retained partition data.
- menu at `0x200`, 1536 original bytes: C implementation using BIOS tick polling in place of original RTC handler.
- third stage at `0xa00`, 2054 original bytes: partial hybrid; original video/mode setup and other bytes remain via incbin.

Identical original binaries occur at multiple paths:

- `/ADMIN/DESKTOP/CMD.X`, `/ICESTAR/CMD.X`.
- `/ADMIN/DESKTOP/TASKMGR.X`, `/ICESTAR/TASKMGR.X`.
