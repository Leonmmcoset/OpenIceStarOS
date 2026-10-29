"""Execute original x86, ordinary C and fixed-address C FAT readers.

Only allocation, pathname lookup and hardware sector reads are intercepted.
Cluster traversal, geometry, first-sector selection and wrapper control flow
execute in full. Real original FAT files and synthetic live-state mutations
check returned bytes, side effects, call order, cdecl preservation and faults.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'
sys.path[:0] = [str(ROOT), str(BUILD / 'test-deps')]
from tools.kernel_image import Fat16, ORIGINAL_SHA256, word
from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
    UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP,
    UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EFLAGS)
from tests.kernel_x_loader import FLAGS, run

SLOTS = [
    ('fat_read_alloc', 'kernel_fat_read_alloc_827454', 0x827454, 0x82760c),
    ('fat_read_into', 'kernel_x_read_path_82760c', 0x82760c, 0x827798),
    ('fat_read_name_alloc', 'kernel_icon_read_absolute_827798', 0x827798, 0x8277dc),
    ('fat_read_name_into', 'kernel_fat_read_named_8277dc', 0x8277dc, 0x827826),
    ('fat_header_relative', 'kernel_x_header_relative_827826', 0x827826, 0x8278f4),
    ('fat_header_path', 'kernel_x_header_path_8278f4', 0x8278f4, 0x82798c),
    ('fat_relative_read', 'kernel_x_read_relative_826ade', 0x826ade, 0x826c3a),
    ('fat_relative_named', 'kernel_fat_read_relative_named_826c3a', 0x826c3a, 0x826c6e),
    ('fat_relative_cluster', 'kernel_fat_read_relative_cluster_826c6e', 0x826c6e, 0x826dd0),
    ('fat_relative_named_cluster', 'kernel_fat_read_relative_named_cluster_826dd0', 0x826dd0, 0x826f38),
    ('fat_relative_alloc_named', 'kernel_icon_read_relative_826f38', 0x826f38, 0x827032),
    ('fat_relative_alloc_index', 'kernel_bitmap_read_relative_827032', 0x827032, 0x82711e),
    ('fat_allocate_index', 'kernel_fat_allocate_index_82711e', 0x82711e, 0x8272f4),
    ('fat_allocate_clusters', 'kernel_fat_allocate_clusters_8272f4', 0x8272f4, 0x827454),
]
HELPERS = {0x815464: ('kernel_allocate', 1),
           0x8263c0: ('kernel_x_find_path_8263c0', 2),
           0x825c8e: ('kernel_x_find_relative_825c8e', 1),
           0x8254ca: ('kernel_fat_read_sectors_8254ca', 3),
           0x828b44: ('kernel_fat_read_sector_828b44', 2)}
BPB, FAT, RECORD, NAME, DEST, ALLOC = 0x520000, 0x500000, 0x600100, 0x600200, 0x610000, 0x620000
STACK, STOP = 0x40f000, 0x200000


def compile_variants(original, installed):
    objects = []
    for module in ('fat_files', 'fat_relative', 'fat_alloc'):
        obj = BUILD / f'kernel_{module}_preview.o'
        run(['gcc', *FLAGS, '-c', f'kernel/{module}.c', '-o', str(obj)])
        objects.append(str(obj))
    outputs, sizes = [], {}
    for fixed in (False, True):
        label = 'fixed' if fixed else 'ordinary'
        script, elf = BUILD / f'fat_files_{label}.ld', BUILD / f'fat_files_{label}.elf'
        sections = '\n'.join(
            f'.{section} {start:#x} : {{ *(.text.{symbol}) }}\n'
            f'ASSERT({symbol} == {start:#x} && SIZEOF(.{section}) <= {end-start}, "slot overflow")'
            for section, symbol, start, end in SLOTS) if fixed else '. = 0x300000; .text : { *(.text*) }'
        script.write_text('OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_fat_read_alloc_827454)\n' +
            '\n'.join(f'PROVIDE({symbol} = {address:#x});' for address, (symbol, _) in HELPERS.items()) +
            '\nSECTIONS {\n' + sections + '\n/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n' +
            '.unexpected : { *(*) }\nASSERT(SIZEOF(.unexpected)==0, "unexpected section")\n}\n')
        run(['ld', '-m', 'elf_i386', '-T', str(script), '-o', str(elf), *objects])
        symbols = {line.split()[-1]: int(line.split()[0], 16)
                   for line in run(['nm', str(elf)], capture_output=True, text=True).stdout.splitlines()}
        if fixed:
            candidate = bytearray(original)
            for section, symbol, start, end in SLOTS:
                path = BUILD / f'fat_files_preview.{section}.bin'
                run(['objcopy', '-O', 'binary', '-j', '.'+section, str(elf), str(path)])
                body = path.read_bytes()
                assert 0 < len(body) <= end-start
                candidate[start-0x800000:end-0x800000] = body + b'\x90'*(end-start-len(body))
                sizes[section] = dict(compiled_bytes=len(body), slot_bytes=end-start)
            if installed:
                production = (BUILD / 'openicestaros_kernel.x').read_bytes()
                for _, _, start, end in SLOTS:
                    assert production[start-0x800000:end-0x800000] == candidate[start-0x800000:end-0x800000]
                candidate = bytearray(production)
            outputs.append((label, bytes(candidate), 0x800000, [symbols[s] for _, s, _, _ in SLOTS]))
        else:
            path = BUILD / 'fat_files_ordinary.bin'
            run(['objcopy', '-O', 'binary', str(elf), str(path)])
            outputs.append((label, path.read_bytes(), 0x300000, [symbols[s] for _, s, _, _ in SLOTS]))
    return outputs, sizes


class Machine:
    def __init__(self, label, binary, base, entries, disk):
        self.label, self.entries, self.disk = label, entries, disk
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        self.uc.mem_map(0, 0x1000000)
        self.uc.mem_write(base, binary)
        self.uc.hook_add(UC_HOOK_CODE, self.finish, begin=STOP, end=STOP)
        for address in HELPERS:
            self.uc.hook_add(UC_HOOK_CODE, self.boundary, begin=address, end=address)

    def put32(self, address, value):
        self.uc.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def finish(self, uc, address, size, unused):
        self.done = True
        uc.emu_stop()

    def boundary(self, uc, address, size, unused):
        label, count = HELPERS[address]
        sp = uc.reg_read(UC_X86_REG_ESP)
        args = list(struct.unpack(f'<{count}I', uc.mem_read(sp+4, count*4)))
        normalized = args.copy()
        result = self.case.get('read_status', 0x7351)
        if label == 'kernel_x_find_path_8263c0':
            normalized[1] = 'record'
            uc.mem_write(args[1], self.record)
            result = self.case.get('lookup', 1)
        elif label == 'kernel_x_find_relative_825c8e':
            result = self.case.get('lookup', self.case.get('index', 1))
            if self.case.get('lookup_mutation'):
                uc.mem_write(BPB+13, b'\x02')
        elif label == 'kernel_allocate':
            result = self.case.get('allocation', ALLOC)
            if self.case.get('allocate_mutation'):
                uc.mem_write(BPB+13, b'\x02')
            if self.case.get('allocate_record_mutation'):
                uc.mem_write(self.root_record+26, struct.pack('<H', 7))
            if self.case.get('allocate_fat_mutation'):
                uc.mem_write(FAT+0x10000, bytes(uc.mem_read(FAT, 0x10000)))
                uc.mem_write(FAT+0x10000+7*2, struct.pack('<H', 0xffff))
                self.put32(0x1a83a, FAT+0x10000)
        else:
            self.reads += 1
            lba, sectors, destination = args if count == 3 else [args[0], 1, args[1]]
            assert sectors <= 32
            length = sectors*512
            assert (0x600000 <= destination <= 0x700000-length or
                    self.case.get('destination') == 0 and destination+length <= 0x10000), args
            if self.case.get('corpus'):
                payload = self.disk[lba*512:(lba+sectors)*512]
                assert len(payload) == length
            else:
                payload = bytes(((lba*19+i*13+self.reads)&255) for i in range(length))
            uc.mem_write(destination, payload)
            if self.reads == 1:
                if self.case.get('bpb_mutation'):
                    uc.mem_write(BPB+11, struct.pack('<H', 256))
                    uc.mem_write(BPB+13, b'\x02')
                if self.case.get('fat_pointer_mutation'):
                    self.put32(0x1a83a, FAT+0x10000)
                if self.case.get('fat_mutation'):
                    uc.mem_write(FAT+7*2, struct.pack('<H', 0xffff))
                if self.case.get('record_mutation'):
                    uc.mem_write(self.root_record+26, struct.pack('<H', 11))
                if self.case.get('partition_mutation'):
                    self.put32(0x7dc6, 0x4321)
        self.trace.append([label, normalized])
        uc.reg_write(UC_X86_REG_EAX, result & 0xffffffff)
        uc.reg_write(UC_X86_REG_ECX, 0x11112222)
        uc.reg_write(UC_X86_REG_EDX, 0x33334444)
        uc.reg_write(UC_X86_REG_ESP, sp+4)
        uc.reg_write(UC_X86_REG_EIP, struct.unpack('<I', uc.mem_read(sp, 4))[0])

    def execute(self, case):
        self.case, self.done, self.trace, self.reads = case, False, [], 0
        uc = self.uc
        uc.mem_write(0, bytes(0x20000))
        uc.mem_write(0x500000, b'\xa5'*0x50000)
        uc.mem_write(0x600000, b'\x96'*0x100000)
        uc.mem_write(STACK-0x4000, bytes(0x5000))
        uc.mem_write(BPB, case['bpb'])
        uc.mem_write(FAT, case['fat'])
        self.put32(0x1a83a, FAT)
        self.put32(0x7dc6, case.get('partition', 17))
        self.record = case['record']
        uc.mem_write(RECORD, self.record)
        uc.mem_write(NAME, b'C:\\ICESTAR\\FILE.X\0')
        bpb = case['bpb']
        root = BPB + (word(bpb, 14)+bpb[16]*word(bpb, 22))*word(bpb, 11)
        index = case.get('index', 0 if case['function'] < 6 else 1)
        directory = case.get('directory', 0x540000) if case['function'] == 12 else root
        self.root_record = (directory + ((index & 0xffffffff) << 5)) & 0xffffffff
        uc.mem_write(self.root_record, self.record)
        # A nonnegative lookup result selects that record, independently of
        # the explicit index used by the direct entries.
        lookup = case.get('lookup', index)
        if 0 <= lookup <= 512:
            uc.mem_write(root+lookup*32, self.record)
        function, destination = case['function'], case.get('destination', DEST)
        skip = case.get('skip', 0)
        args = ([RECORD], [RECORD, destination], [NAME], [NAME, destination],
                [index, destination], [RECORD, destination], [index, destination],
                [NAME, destination], [index, destination, skip],
                [NAME, destination, skip], [NAME], [index], [directory, index],
                [case.get('cluster', word(self.record, 26)), case.get('requested', 2)])[function]
        args = [v & 0xffffffff for v in args]
        uc.mem_write(STACK, struct.pack(f'<{len(args)+3}I', STOP, *args, 0x13579bdf, 0x2468ace0))
        saved = [(UC_X86_REG_EBX, 0x12345678), (UC_X86_REG_ESI, 0x87654321),
                 (UC_X86_REG_EDI, 0xabcdef12), (UC_X86_REG_EBP, 0x11223344)]
        for reg, value in [*saved, (UC_X86_REG_ESP, STACK), (UC_X86_REG_EFLAGS, 0x202)]:
            uc.reg_write(reg, value)
        fault = None
        try:
            uc.emu_start(self.entries[function], STOP+1, count=100000)
        except UcError as error:
            fault = error.errno
            assert case.get('fault'), f'{self.label} {case.get("name")} fault at {uc.reg_read(UC_X86_REG_EIP):#x}: {error}'
        if not fault:
            assert self.done, (self.label, 'instruction budget exhausted',
                               {k:v for k,v in case.items() if not isinstance(v, bytes)},
                               hex(uc.reg_read(UC_X86_REG_EIP)), self.trace[-5:])
            assert uc.reg_read(UC_X86_REG_ESP) == STACK+4
            assert all(uc.reg_read(r) == v for r, v in saved), 'callee-saved register changed'
            assert bytes(uc.mem_read(STACK+4, len(args)*4+8)) == struct.pack(f'<{len(args)+2}I', *args, 0x13579bdf, 0x2468ace0)
        assert bool(fault) == bool(case.get('fault')), 'expected division fault was not delivered'
        output = bytes(uc.mem_read(0x600000, 0x100000))
        if case.get('corpus') and function in (0, 1, 2, 3, 6, 7, 10, 11, 12, 13):
            dest = ALLOC if function in (0, 2, 10, 11, 12, 13) else destination
            expected = case['expected']
            assert output[dest-0x600000:dest-0x600000+len(expected)] == expected, 'corpus bytes differ'
        if 'expected_return' in case:
            assert uc.reg_read(UC_X86_REG_EAX) == case['expected_return'] & 0xffffffff
        if 'expected_reads' in case:
            assert self.reads == case['expected_reads'], (self.label, self.trace)
        if 'expected_allocation' in case:
            allocations = [args[0] for label, args in self.trace if label == 'kernel_allocate']
            assert allocations == [case['expected_allocation']], (self.label, self.trace)
        if 'expected_trace' in case:
            assert self.trace == case['expected_trace'], (self.label, self.trace)
        if 'expected_allocated_bytes' in case:
            payload = case['expected_allocated_bytes']
            assert bytes(uc.mem_read(ALLOC, len(payload)+32)) == payload + b'\x96'*32
        if 'expected_slice' in case:
            offset, payload = case['expected_slice']
            assert bytes(uc.mem_read(destination+offset, len(payload))) == payload, 'cluster bytes differ'
        return (fault, None if fault else uc.reg_read(UC_X86_REG_EAX), self.trace,
                bytes(uc.mem_read(0, 0x20000)), bytes(uc.mem_read(0x500000, 0x50000)), output)


def synthetic(function, *, bps=512, spc=1, entries=17, size=700, chain=(5, 7, 9), **values):
    bpb = bytearray(512)
    struct.pack_into('<H', bpb, 11, bps)
    bpb[13] = spc
    struct.pack_into('<H', bpb, 14, 1)
    bpb[16] = 2
    struct.pack_into('<H', bpb, 17, entries)
    struct.pack_into('<H', bpb, 22, 4)
    fat = bytearray(0x20000)
    for index, cluster in enumerate(chain):
        struct.pack_into('<H', fat, cluster*2, chain[index+1] if index+1 < len(chain) else 0xffff)
    record = bytearray(32)
    struct.pack_into('<H', record, 26, chain[0])
    struct.pack_into('<I', record, 28, size)
    record[11] = values.pop('attributes', 0)
    return dict(function=function, bpb=bytes(bpb), fat=bytes(fat), record=bytes(record), **values)


def corpus_record(disk, names):
    contents = disk.root
    for index, name in enumerate(names):
        entry = disk.find(contents, name, index+1 < len(names))
        if index+1 < len(names):
            contents = b''.join(disk.data[o:o+disk.cluster_bytes] for o in disk.chain(word(entry, 26)))
    return entry


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--installed', action='store_true')
    args = parser.parse_args()
    started = time.monotonic()
    source = (ROOT.parent / 'IceStarOS_x86_32M.vhd').read_bytes()
    disk = Fat16(source)
    original = disk.read_kernel()
    assert hashlib.sha256(original).hexdigest() == ORIGINAL_SHA256
    variants, sizes = compile_variants(original, args.installed)
    specs = [('original', original, 0x800000, [s[2] for s in SLOTS]), *variants]
    machines = [Machine(*v, source) for v in specs]
    cases = []
    for function in range(6):
        for bps in (128, 512, 1024):
            for spc in (1, 2, 16):
                for entries in (0, 1, 17, 512):
                    cases.append(synthetic(function, bps=bps, spc=spc, entries=entries))
        for size in (0, 1, 511, 512, 513, 0x7fffffff, 0xffffffff):
            for attributes in (0, 16, 0xff):
                cases.append(synthetic(function, size=size, attributes=attributes))
        for option, values in dict(lookup=(0, 2, 0x7fffffff, 0x80000000, 0xffffffff),
            allocation=(0,), destination=(0,), bpb_mutation=(True,),
            fat_pointer_mutation=(True,), fat_mutation=(True,), allocate_mutation=(True,),
            index=(1, 7), read_status=(0, 0xffffffff)).items():
            for value in values:
                if option == 'destination' and function >= 4:
                    continue
                cases.append(synthetic(function, **{option:value}))
        for chain in ((5,), (5, 0xfff8), (5, 0, 1, 9)):
            cases.append(synthetic(function, chain=chain))
        # Division occurs even for null destinations and failed allocations.
        for bps, spc in ((0, 1), (512, 0)):
            if function >= 4 and spc == 0:
                continue  # header readers divide only by bytes-per-sector
            cases.append(synthetic(function, bps=bps, spc=spc, fault=True, destination=0,
                                   lookup=1, allocation=0))
    for function in range(6, 12):
        for bps in (128, 512, 1024):
            for spc in (1, 2, 16):
                for entries in (0, 1, 17, 512):
                    cases.append(synthetic(function, bps=bps, spc=spc, entries=entries))
        for size in (0, 1, 511, 512, 513, 0x7fffffff, 0xffffffff):
            for attributes in (0, 16, 0xff):
                cases.append(synthetic(function, size=size, attributes=attributes))
        for option, values in dict(lookup=(0, 1, 7, 0x80000000, 0xffffffff),
            allocation=(0,), destination=(0,), bpb_mutation=(True,),
            fat_pointer_mutation=(True,), fat_mutation=(True,), allocate_mutation=(True,),
            lookup_mutation=(True,), record_mutation=(True,), partition_mutation=(True,),
            index=(0, 1, 7, -1), read_status=(0, 0xffffffff), partition=(0, 0xfffffff0)).items():
            for value in values:
                cases.append(synthetic(function, **{option:value}))
        for chain in ((5,), (5, 0xfff8), (5, 0, 1, 9)):
            for skip in (-1, 0, 1, 2, 3, 0x7fffffff):
                cases.append(synthetic(function, chain=chain, skip=skip))
        # Direct and named single-cluster readers deliberately disagree when
        # asked for a negative offset or a cluster past the end of the chain.
        if function in (8, 9):
            for skip, walked in ((-1, 0), (0, 0), (1, 1), (2, 2), (3, 2)):
                cases.append(synthetic(function, skip=skip, expected_return=walked,
                    expected_reads=int(function == 9 or walked == skip)))
        # Index 0 is accepted by whole-file/allocating readers but rejected
        # by both single-cluster entries and the named whole-file wrapper.
        cases.append(synthetic(function, index=0, lookup=0,
            expected_reads=0 if function in (7, 8, 9) else 3))
        for bps, spc in ((0, 1), (512, 0)):
            cases.append(synthetic(function, bps=bps, spc=spc,
                fault=bps == 0 or function in (10, 11), lookup=1, allocation=0))
    for function in (12, 13):
        for bps in (128, 512, 1024):
            for spc in (1, 2, 16):
                for entries in (0, 1, 17, 512):
                    cases.append(synthetic(function, bps=bps, spc=spc, entries=entries))
        for size in (0, 1, 511, 512, 513, 0x7fffffff, 0xffffffff):
            for attributes in (0, 16, 0xff):
                clusters = ((512 + size - 1) & 0xffffffff)//512
                count = (1 if attributes & 16 else clusters) if function == 12 else 2
                cases.append(synthetic(function, size=size, attributes=attributes,
                    expected_allocation=(count*512) & 0xffffffff, expected_reads=3 if function == 12 else 2))
        for option, values in dict(allocation=(0,), bpb_mutation=(True,),
            fat_pointer_mutation=(True,), fat_mutation=(True,), allocate_mutation=(True,),
            allocate_fat_mutation=(True,), record_mutation=(True,), partition_mutation=(True,),
            read_status=(0, 0xffffffff), partition=(0, 0xfffffff0)).items():
            for value in values:
                cases.append(synthetic(function, **{option:value}))
        for chain in ((5,), (5, 0xfff8), (5, 0, 1, 9)):
            cases.append(synthetic(function, chain=chain))
    for directory in (0x540000, 0x540003, RECORD, 0x600000):
        for index in (0, 1, 7, 0x08000001, -1, -0x80000000):
            for attributes in (0, 16):
                cases.append(synthetic(12, directory=directory, index=index, attributes=attributes,
                    expected_return=0 if index < 0 else ALLOC, expected_reads=0 if index < 0 else 3,
                    **({'expected_trace': []} if index < 0 else {})))
    cases.append(synthetic(12, allocate_record_mutation=True, expected_reads=2))
    for requested in (0, 1, 2, 3, 4, 0x7fffffff, 0x80000000, 0xffffffff):
        cases.append(synthetic(13, requested=requested, expected_reads=min(max(requested, 1), 3),
            expected_allocation=(requested*512) & 0xffffffff, expected_return=ALLOC))
    # Exact boundary traces check allocation before reads, the zero-count
    # first read, empty-root sector rounding, and the cached data LBA.
    cases.append(synthetic(13, requested=0, entries=0, expected_trace=[
        ['kernel_allocate', [0]], ['kernel_fat_read_sectors_8254ca', [29, 1, ALLOC]]]))
    for function in (12, 13):
        for bps, spc in ((0, 1), (512, 0)):
            for allocation in (0, ALLOC):
                cases.append(synthetic(function, bps=bps, spc=spc, allocation=allocation,
                    fault=(function == 12 or (bps == 0 and allocation != 0)),
                    **({'expected_trace': []} if function == 12 else {})))
    cases.append(synthetic(12, index=-1, bps=0, spc=0, expected_return=0, expected_trace=[]))
    for program in json.loads((ROOT / 'recovery/inventory.json').read_text())['programs']:
        names = tuple(n.encode('ascii') for n in program['short_names'])
        record = corpus_record(disk, names)
        expected = disk.read_file(*names)
        for function in range(13):
            cases.append(dict(function=function, bpb=source[17*512:18*512],
                fat=disk.fat.ljust(0x20000, b'\0'), record=record, corpus=True,
                expected=expected, name=program['path']))
        chain = list(disk.chain(word(record, 26)))
        for requested in (0, 1, len(chain)//2, len(chain), len(chain)+1, 0x80000000):
            raw = b''.join(source[o:o+disk.cluster_bytes] for o in chain[:max(requested, 1)])
            cases.append(dict(function=13, bpb=source[17*512:18*512],
                fat=disk.fat.ljust(0x20000, b'\0'), record=record, corpus=True,
                name=program['path'], requested=requested, expected=raw,
                expected_allocated_bytes=raw, expected_reads=min(max(requested, 1), len(chain)),
                expected_allocation=(requested*disk.cluster_bytes) & 0xffffffff))
        for function in (8, 9):
            for skip in (0, len(chain)//2, len(chain)-1, len(chain), -1):
                walked = min(max(skip, 0), len(chain)-1)
                reads = function == 9 or walked == skip
                case = dict(function=function, bpb=source[17*512:18*512],
                    fat=disk.fat.ljust(0x20000, b'\0'), record=record, corpus=True,
                    name=program['path'], skip=skip, expected_return=walked,
                    expected_reads=int(reads))
                if reads:
                    offset = chain[walked]
                    case['expected_slice'] = (walked*disk.cluster_bytes,
                                              source[offset:offset+disk.cluster_bytes])
                cases.append(case)
    for index, case in enumerate(cases):
        # A delivered x86 exception leaves Unicorn's CPU exception state live;
        # isolate fault probes instead of carrying that state into later calls.
        current = [Machine(*v, source) for v in specs] if case.get('fault') else machines
        expected = current[0].execute(case)
        for machine in current[1:]:
            actual = machine.execute(case)
            assert actual == expected, (index,
                {k:v for k,v in case.items() if not isinstance(v, bytes)}, machine.label,
                [(i, a[:12] if isinstance(a, bytes) else a, b[:12] if isinstance(b, bytes) else b)
                 for i, (a, b) in enumerate(zip(expected, actual)) if a != b])
    summary = dict(installed=args.installed, scenarios=len(cases), executions=len(cases)*3,
        corpus_paths=39, slots=sizes, seconds=round(time.monotonic()-started, 3),
        boundary_scope='allocation, pathname lookup and hardware sector reads only',
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest()
                for p in ['kernel/fat_files.c', 'kernel/fat_files.h',
                          'kernel/fat_relative.c', 'kernel/fat_relative.h',
                          'kernel/fat_alloc.c', 'kernel/fat_alloc.h', 'tests/kernel_fat_files.py']})
    (BUILD / 'fat_files_summary.json').write_text(json.dumps(summary, indent=2)+'\n')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
