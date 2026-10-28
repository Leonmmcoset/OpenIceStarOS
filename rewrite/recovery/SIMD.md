# Vector arithmetic recovery

The four leaves at `0x81ab34..0x81acc4` accept three stack pointers to 16
bytes. Both input vectors are read before any output store. Loads and stores
are unaligned. All general registers, stack arguments and EFLAGS survive;
the XMM registers changed by the original instructions remain changed.
`kernel/simd.c` uses named inline assembler inside naked C definitions to
retain this interface. There is no copied binary body or external library.

The first three functions write their third argument. For each byte lane:

| Entry | Arithmetic |
| --- | --- |
| `0x81ab34` | `(left + right) modulo 256` |
| `0x81ab58` | `(left - right) modulo 256` |
| `0x81ab7c` | `p = left * right; p >= 32768 ? 0 : min(p, 255)` |

The multiply widens unsigned bytes and uses `PMULLW`, then **signed** word
input to `PACKUSWB`. Thus `255 * 255` produces zero, while `128 * 255`
produces 255. Its `PMOVZXBW` instructions require SSE4.1. Add, subtract and
blend require SSE2. These are the original CPU requirements; this recovery
adds no dispatch or fallback.

`0x81abc6` writes its first argument. Let a weight DWORD have little-endian
bytes `a, b, c, d`. Its four effective lane weights are
`a, a|b, a|b|c, b|c|d`. For a destination byte `x`, source byte `y`, effective
weight `w`, and channel constant `k` (128 for B/G/R, zero for alpha), the
result is:

```c
unsigned inverse = (k - w) & 255;
unsigned sum = (x * w + y * inverse) & 65535;
unsigned result = sum >> 7;
output = result > 255 ? 255 : result;
```

All sixteen result bytes, including alpha, are stored together. The function
does not clamp weights or skip transparent pixels. This is not equivalent to
the scalar service at `0x818134` for arbitrary inputs. Service-table slot 18
selects this SSE2 leaf when CPU feature bit 25 is set. Its ordinary rendering
caller passes four DWORD weights in the range 0..128; the upper three bytes
of each such weight are zero.
