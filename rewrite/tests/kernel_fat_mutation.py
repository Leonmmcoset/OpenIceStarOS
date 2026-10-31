"""Compare FAT chain release/deletion: original x86, ordinary C and installed C.

Only name lookup and metadata flush are intercepted. Chain traversal, record
addressing and deletion wrappers execute in full. Compare ordered writes,
source snapshots, return values and cdecl; real file records seed corpus cases.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
sys.path[:0] = [str(ROOT), str(BUILD / "test-deps")]
from tools.kernel_image import Fat16, ORIGINAL_SHA256, word
from tests.kernel_x_loader import FLAGS, run
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
    UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP,
    UC_X86_REG_ESP, UC_X86_REG_EIP)

SLOTS = [
    ("fat_release_root_chain", "kernel_fat_release_root_chain_827d4a", 0x827d4a, 0x827e6c),
    ("fat_release_record_chain", "kernel_fat_release_record_chain_827e6c", 0x827e6c, 0x827f58),
    ("fat_delete_root", "kernel_fat_delete_root_827f58", 0x827f58, 0x827fe2),
    ("fat_delete_relative", "kernel_fat_delete_relative_827fe2", 0x827fe2, 0x828018),
]
BASE, BPB, FAT1, FAT2 = 0x800000, 0x520000, 0x600000, 0x640000
RECORD, NAME, STACK, STOP = 0x580003, 0x590000, 0x40f000, 0x200000
MASK = 0xffffffff


def compile_variants(original, installed):
    obj = BUILD / "kernel_fat_mutation_preview.o"
    run(["gcc", *FLAGS, "-c", "kernel/fat_mutation.c", "-o", str(obj)])
    outputs, sizes = [], {}
    for fixed in (False, True):
        label = "fixed" if fixed else "ordinary"
        script, elf = BUILD / f"fat_mutation_{label}.ld", BUILD / f"fat_mutation_{label}.elf"
        sections = "\n".join(
            f".{name} {start:#x} : {{ *(.text.{symbol}) }}\n"
            f"ASSERT({symbol} == {start:#x} && SIZEOF(.{name}) <= {end-start}, \"slot overflow\")"
            for name, symbol, start, end in SLOTS) if fixed else ". = 0x300000; .text : { *(.text*) }"
        script.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_fat_release_root_chain_827d4a)\n" +
            "PROVIDE(kernel_fat_flush_metadata_827cc4 = 0x827cc4);\n" +
            "PROVIDE(kernel_x_find_relative_825c8e = 0x825c8e);\nSECTIONS {\n" +
            sections + "\n/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n" +
            ".unexpected : { *(*) }\nASSERT(SIZEOF(.unexpected)==0, \"unexpected section\")\n}\n")
        run(["ld", "-m", "elf_i386", "-T", str(script), "-o", str(elf), str(obj)])
        symbols = {line.split()[-1]: int(line.split()[0], 16)
                   for line in run(["nm", str(elf)], capture_output=True, text=True).stdout.splitlines()}
        if fixed:
            candidate = bytearray(original)
            for name, symbol, start, end in SLOTS:
                path = BUILD / f"fat_mutation_preview.{name}.bin"
                run(["objcopy", "-O", "binary", "-j", "." + name, str(elf), str(path)])
                body = path.read_bytes()
                assert 0 < len(body) <= end - start
                candidate[start-BASE:end-BASE] = body + b"\x90" * (end-start-len(body))
                sizes[name] = {"compiled_bytes": len(body), "slot_bytes": end-start}
            if installed:
                production = (BUILD / "openicestaros_kernel.x").read_bytes()
                for _, _, start, end in SLOTS:
                    assert production[start-BASE:end-BASE] == candidate[start-BASE:end-BASE]
                candidate = bytearray(production)
            path = BUILD / "fat_mutation_candidate.x"
            path.write_bytes(candidate)
            outputs.append(("fixed", bytes(candidate), BASE,
                            [symbols[symbol] for _, symbol, _, _ in SLOTS]))
        else:
            path = BUILD / "fat_mutation_ordinary.bin"
            run(["objcopy", "-O", "binary", str(elf), str(path)])
            outputs.append(("ordinary", path.read_bytes(), 0x300000,
                            [symbols[symbol] for _, symbol, _, _ in SLOTS]))
    return outputs, sizes


class Machine:
    def __init__(self, label, binary, base, entries):
        self.label, self.entries = label, entries
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        self.uc.mem_map(0, 0x1000000)
        self.uc.mem_write(base, binary)
        self.uc.hook_add(UC_HOOK_CODE, self.finish, begin=STOP, end=STOP)
        self.uc.hook_add(UC_HOOK_CODE, self.flush, begin=0x827cc4, end=0x827cc4)
        self.uc.hook_add(UC_HOOK_CODE, self.lookup, begin=0x825c8e, end=0x825c8e)
        self.uc.hook_add(UC_HOOK_MEM_WRITE, self.write, begin=0x500000, end=0x700000)

    def finish(self, uc, address, size, unused):
        self.done = True
        uc.emu_stop()

    def write(self, uc, access, address, size, value, unused):
        self.writes.append((address, size, value))
        # Mutate after a FAT1 write: FAT2's pointer is cached, its starting
        # cluster is reread, and its enable byte in the BPB remains live.
        mutation = self.case.get('on_write')
        if mutation and len(self.writes) == 1:
            uc.mem_write(0x1a83e, struct.pack('<I', FAT2 + 0x10000))
            if mutation == 'fats':
                uc.mem_write(BPB+16, b'\x01')
            elif mutation == 'record':
                uc.mem_write(self.record+26, struct.pack('<H', 13))

    def returned(self, value):
        uc = self.uc
        sp = uc.reg_read(UC_X86_REG_ESP)
        uc.reg_write(UC_X86_REG_EAX, value & MASK)
        uc.reg_write(UC_X86_REG_ECX, 0x11112222)
        uc.reg_write(UC_X86_REG_EDX, 0x33334444)
        uc.reg_write(UC_X86_REG_ESP, sp+4)
        uc.reg_write(UC_X86_REG_EIP, struct.unpack('<I', uc.mem_read(sp, 4))[0])

    def lookup(self, uc, address, size, unused):
        sp = uc.reg_read(UC_X86_REG_ESP)
        name = struct.unpack('<I', uc.mem_read(sp+4, 4))[0]
        assert name == NAME
        self.calls.append(('lookup', name))
        self.returned(self.case['lookup'])

    def flush(self, uc, address, size, unused):
        # Observe state at the side-effect boundary. Deletion must precede
        # writeback; no FAT release is part of this call path.
        self.calls.append(('flush', self.record, bytes(uc.mem_read(self.record, 32))))
        self.returned(self.case.get('flush_eax', 0xdeadbeef))

    def execute(self, function, case):
        self.case, self.done, self.writes, self.calls = case, False, [], []
        uc = self.uc
        uc.mem_write(0x500000, bytes(0x200000))
        uc.mem_write(0x1a83a, struct.pack('<II', FAT1, case.get('fat2', FAT2)))
        bpb = bytearray(512)
        bps, reserved, fats, sectors = [case.get(k, v) for k, v in
            (('bps', 512), ('reserved', 1), ('fats', 2), ('fat_sectors', 16))]
        struct.pack_into('<H', bpb, 11, bps)
        struct.pack_into('<H', bpb, 14, reserved)
        bpb[16] = fats
        struct.pack_into('<H', bpb, 22, sectors)
        uc.mem_write(BPB, bytes(bpb))
        index = case.get('index', 2) if function != 3 else case['lookup']
        root = (BPB + (reserved + fats * sectors) * bps) & MASK
        self.record = RECORD if function == 1 else (root + ((index << 5) & MASK)) & MASK
        if function == 3 and (index == 0 or index & 0x80000000):
            self.record = root + 64
        record = bytearray(case.get('record', bytes(range(32))))
        record[0] = case.get('first', 0x41)
        struct.pack_into('<H', record, 20, case.get('high', 0))
        struct.pack_into('<H', record, 26, case.get('start', 2))
        uc.mem_write(self.record, bytes(record))
        for fat, key in ((FAT1, 'chain1'), (case.get('fat2', FAT2), 'chain2')):
            for cluster, next_cluster in case.get(key, {2:3, 3:0xffff}).items():
                address = (fat + ((cluster << 1) & MASK)) & MASK
                uc.mem_write(address, struct.pack('<H', next_cluster))
        uc.mem_write(NAME, b'FIXTURE.X\0')
        before = bytes(uc.mem_read(0x500000, 0x200000))
        arg = RECORD if function == 1 else NAME if function == 3 else index
        uc.mem_write(STACK-0x1000, bytes(0x2000))
        uc.mem_write(STACK, struct.pack('<4I', STOP, arg & MASK, 0x13579bdf, 0x2468ace0))
        saved = [(UC_X86_REG_EBX, 0x12345678), (UC_X86_REG_ESI, 0x87654321),
                 (UC_X86_REG_EDI, 0xabcdef12), (UC_X86_REG_EBP, 0x11223344)]
        for reg, value in [*saved, (UC_X86_REG_EAX, 0x99887766),
                           (UC_X86_REG_ECX, 0x55443322), (UC_X86_REG_EDX, 0xdeadbeef),
                           (UC_X86_REG_ESP, STACK)]:
            uc.reg_write(reg, value)
        uc.emu_start(self.entries[function], STOP+1, count=100000)
        assert self.done, (self.label, function, case, hex(uc.reg_read(UC_X86_REG_EIP)))
        assert uc.reg_read(UC_X86_REG_ESP) == STACK+4
        assert all(uc.reg_read(reg) == value for reg, value in saved)
        assert bytes(uc.mem_read(STACK+4, 12)) == struct.pack('<3I', arg & MASK, 0x13579bdf, 0x2468ace0)
        after = bytes(uc.mem_read(0x500000, 0x200000))
        # Independently check the changed-byte set and return contract. Ordered
        # writes detect clear-before-follow bugs and distinct FAT chain order.
        if not case.get('on_write'):
            model = bytearray(before)
            changes = []
            if function < 2:
                start = ((case.get('high', 0) << 16) + case.get('start', 2)) & MASK
                for fat in ([FAT1] if fats else []) + ([case.get('fat2', FAT2)] if fats > 1 else []):
                    current = start
                    for _ in range(65540):
                        address = (fat + ((current << 1) & MASK)) & MASK
                        next_cluster = struct.unpack_from('<H', model, address-0x500000)[0]
                        struct.pack_into('<H', model, address-0x500000, 0)
                        changes.append((address, 2, 0))
                        if next_cluster in (0, 0xffff):
                            break
                        current = next_cluster
                    else:
                        raise AssertionError('unbounded model chain')
                expected_return = fats
            else:
                valid = function == 2 or (0 < case['lookup'] < 0x80000000)
                valid = valid and record[0] not in (0, 0xe5)
                expected_return = 1 if valid else MASK
                if valid:
                    model[self.record-0x500000] = 0xe5
                    changes.append((self.record, 1, 0xe5))
                assert sum(c[0] == 'flush' for c in self.calls) == int(valid)
            assert uc.reg_read(UC_X86_REG_EAX) == expected_return
            assert self.writes == changes, (self.label, function, case, self.writes, changes)
            assert after == model, (self.label, function, case, 'unexpected memory modification')
        return (uc.reg_read(UC_X86_REG_EAX), self.writes, self.calls, hashlib.sha256(after).hexdigest())


def cases(disk):
    result = []
    chains = [
        ({2:0}, {2:0xffff}), ({2:0xffff}, {2:5, 5:0}),
        ({2:3, 3:7, 7:0xffff}, {2:5, 5:9, 9:0xffff}),
        ({2:2}, {2:2}), ({2:3, 3:2}, {2:4, 4:2}),
        ({2:0xfff8, 0xfff8:0}, {2:0xfff0, 0xfff0:0xffff}),
        ({2:1, 1:0}, {2:0xfffe, 0xfffe:0xffff}),
    ]
    for function in (0, 1):
        for fats in (0, 1, 2, 3, 255):
            for first, second in chains:
                result.append((function, dict(fats=fats, fat_sectors=0 if fats == 255 else 16,
                    chain1=first, chain2=second)))
        for high in (1, 0x8000, 0xffff):
            start = (high << 16) + 2
            result.append((function, dict(high=high, chain1={start:0xffff}, chain2={start:0})))
        for start in (0, 1, 0xffff):
            result.append((function, dict(start=start, chain1={start:0}, chain2={start:0xffff})))
        for mutation in ('pointer', 'fats', 'record'):
            result.append((function, dict(on_write=mutation, chain2={2:3, 3:0xffff, 13:0xffff})))
        result.append((function, dict(fat2=FAT1)))
    for function in (0, 2):
        for index in (0, 1, 15, 0x80000000, 0xffffffff):
            result.append((function, dict(index=index)))
        for bps, reserved, sectors in ((0,1,16), (1,0,0), (128,17,1), (1024,0,8), (65535,0,0)):
            result.append((function, dict(bps=bps, reserved=reserved, fat_sectors=sectors)))
    for first in range(256):
        result.append((2, dict(first=first)))
    for lookup in (0, 1, 2, 31, 0x7ffffff, 0x80000000, 0xffffffff):
        for first in (0, 0xe5, 0x41, 0xff):
            result.append((3, dict(lookup=lookup, first=first, flush_eax=0)))
    # All original program records, traversing each original FAT copy in RAM.
    inventory = json.loads((ROOT/'recovery/inventory.json').read_text())
    for program in inventory['programs']:
        names = [n.encode('ascii') for n in program['short_names']]
        directory = disk.root
        for name in names[:-1]:
            entry = disk.find(directory, name, True)
            directory = b''.join(disk.data[o:o+disk.cluster_bytes] for o in disk.chain(word(entry,26)))
        record = disk.find(directory, names[-1], False)
        start = word(record, 26)
        for function in (0, 1):
            tables = []
            for fat in [disk.fat, *disk.fat_copies]:
                chain = {}
                current = start
                while current not in chain and 0 < current < len(fat)//2:
                    value = word(fat, current*2)
                    chain[current] = value
                    if value in (0,0xffff):
                        break
                    current = value
                tables.append(chain)
            result.append((function, dict(record=record, start=start, chain1=tables[0], chain2=tables[1],
                                          path=program['path'])))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installed', action='store_true')
    args = parser.parse_args()
    started = time.monotonic()
    disk = Fat16((ROOT.parent/'IceStarOS_x86_32M.vhd').read_bytes())
    original = disk.read_kernel()
    assert hashlib.sha256(original).hexdigest() == ORIGINAL_SHA256
    variants, sizes = compile_variants(original, args.installed)
    specs = [('original', original, BASE, [s[2] for s in SLOTS]), *variants]
    machines = [Machine(*spec) for spec in specs]
    all_cases = cases(disk)
    for number, (function, case) in enumerate(all_cases):
        expected = machines[0].execute(function, case)
        for machine in machines[1:]:
            actual = machine.execute(function, case)
            assert actual == expected, (number, function, case, machine.label, expected, actual)
    summary = dict(installed=args.installed, scenarios=len(all_cases), executions=len(all_cases)*3,
        corpus_paths=39, slots=sizes, seconds=round(time.monotonic()-started,3),
        scope='Only pathname lookup and metadata flush are intercepted.',
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in
                ('kernel/fat_mutation.c', 'kernel/fat_mutation.h', 'tests/kernel_fat_mutation.py')})
    (BUILD/'fat_mutation_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2))


if __name__ == '__main__':
    main()
