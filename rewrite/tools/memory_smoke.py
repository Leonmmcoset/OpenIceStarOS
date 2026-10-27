"""Inspect RAM-detection metadata and the live heap chain without guest writes."""

import json
import struct

from kernel_image import require


def check_memory(execute, build, expected_limit=128 * 1024 * 1024):
    scratch = build / "heap_state.memory"

    def read_paused(address, size):
        scratch.unlink(missing_ok=True)
        execute(f'pmemsave {address:#x} {size:#x} "{scratch}"')
        data = scratch.read_bytes()
        require(len(data) == size, "incomplete heap state read")
        return data

    execute("stop")
    try:
        limit, pages, page_size, root = struct.unpack("<4I", read_paused(0x1b050, 16))
        require(limit == expected_limit, "RAM probe did not detect the configured guest memory")
        require(pages == limit // 4096 and page_size == 4096, "incorrect heap page metadata")
        require(root == 0x2000000, "incorrect heap origin")
        node, seen, headers = root, set(), []
        while True:
            require(root <= node <= limit - 16 and node not in seen,
                    "heap node outside RAM or cyclic next chain")
            require(len(headers) < 65536, "unexpectedly long heap chain")
            seen.add(node)
            following, previous, used, owner = struct.unpack("<4I", read_paused(node, 16))
            require(used in (0, 1), "invalid live heap allocation state")
            headers.append({"address": hex(node), "next": hex(following),
                            "previous": hex(previous), "used": used, "owner": owner})
            if following == 0:
                require(used == 0, "heap has no free tail marker")
                break
            require(node + 16 <= following <= limit - 16,
                    "live heap extent is reversed, undersized, or outside RAM")
            node = following
        require(any(header["used"] == 1 for header in headers), "no live heap allocation observed")
        # Successor back-links are recorded but not asserted: the original
        # allocator/free routines deliberately do not maintain all of them.
        evidence = {"memory_limit": limit, "page_count": pages, "page_size": page_size,
                    "heap_origin": hex(root), "headers": headers}
        (build / "memory_smoke.json").write_text(json.dumps(evidence, indent=2) + "\n")
    finally:
        execute("cont")
    print(f"RAM probe detected {limit} bytes; {len(headers)} live heap headers verified")
