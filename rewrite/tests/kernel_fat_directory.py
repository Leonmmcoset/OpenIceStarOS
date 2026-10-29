"""Compare directory services with original x86, ordinary C and installed C.

No service is stubbed: extraction, lookup, attribute calls and string equality
execute as x86 against real directory bytes. Code hooks only record calls and
apply explicit live-state mutations at a string-comparison entry.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'
sys.path[:0] = [str(ROOT), str(BUILD / 'test-deps')]
from tools.kernel_image import Fat16, ORIGINAL_SHA256, word
from tests.kernel_x_loader import FLAGS, run
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
    UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP,
    UC_X86_REG_ESP, UC_X86_REG_EFLAGS)

SLOTS = [
    ('fat_root_name', 'kernel_fat_root_name_8258a6', 0x8258a6, 0x825a7e),
    ('fat_directory_name', 'kernel_fat_directory_name_825a7e', 0x825a7e, 0x825bcc),
    ('fat_root_extension', 'kernel_fat_root_extension_825bcc', 0x825bcc, 0x825c46),
    ('fat_directory_extension', 'kernel_fat_directory_extension_825c46', 0x825c46, 0x825c8e),
    ('fat_find_relative', 'kernel_x_find_relative_825c8e', 0x825c8e, 0x825d34),
    ('fat_directory_find', 'kernel_fat_directory_find_825d34', 0x825d34, 0x825df4),
    ('fat_root_size', 'kernel_fat_root_size_826940', 0x826940, 0x82698c),
    ('fat_directory_size', 'kernel_fat_directory_size_82698c', 0x82698c, 0x8269a6),
    ('fat_root_attributes', 'kernel_fat_root_attributes_8269a6', 0x8269a6, 0x8269f2),
    ('fat_directory_attributes', 'kernel_fat_directory_attributes_8269f2', 0x8269f2, 0x826a0e),
    ('fat_root_date', 'kernel_fat_root_date_826a0e', 0x826a0e, 0x826a5a),
    ('fat_directory_date', 'kernel_fat_directory_date_826a5a', 0x826a5a, 0x826a76),
    ('fat_root_time', 'kernel_fat_root_time_826a76', 0x826a76, 0x826ac2),
    ('fat_directory_time', 'kernel_fat_directory_time_826ac2', 0x826ac2, 0x826ade),
    ('fat_root_find_directory', 'kernel_fat_root_find_directory_825df4', 0x825df4, 0x825e86),
    ('fat_find_directory', 'kernel_fat_find_directory_825e86', 0x825e86, 0x825f48),
]
BPB, DIRECTORY, NAME, OUTPUT, STACK, STOP = 0x520000, 0x580000, 0x700100, 0x710100, 0x40f000, 0x200000
ARGC = (2, 3, 2, 3, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2)


def describe(case):
    return {key: f'{value[:32].hex()}... ({len(value)} bytes)' if isinstance(value, bytes) else value
            for key, value in case.items()}


def compile_variants(original, installed):
    obj = BUILD / 'kernel_fat_directory_preview.o'
    run(['gcc', *FLAGS, '-c', 'kernel/fat_directory.c', '-o', str(obj)])
    variants, sizes = [], {}
    for fixed in (False, True):
        label = 'fixed' if fixed else 'ordinary'
        script, elf = BUILD/f'fat_directory_{label}.ld', BUILD/f'fat_directory_{label}.elf'
        sections = '\n'.join(
            f'.{section} {start:#x} : {{ *(.text.{symbol}) }}\n'
            f'ASSERT({symbol}=={start:#x} && SIZEOF(.{section})<={end-start}, "slot overflow")'
            for section, symbol, start, end in SLOTS) if fixed else '. = 0x300000; .text : { *(.text*) }'
        script.write_text('OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_x_find_relative_825c8e)\n'
            'PROVIDE(kernel_strings_equal = 0x82a481);\nSECTIONS {\n'+sections+
            '\n/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n'
            '.unexpected : { *(*) }\nASSERT(SIZEOF(.unexpected)==0,"unexpected section")\n}\n')
        run(['ld', '-m', 'elf_i386', '-T', str(script), '-o', str(elf), str(obj)])
        symbols = {line.split()[-1]: int(line.split()[0], 16)
                   for line in run(['nm', str(elf)], capture_output=True, text=True).stdout.splitlines()}
        candidate = bytearray(original)
        if fixed:
            for section, _, start, end in SLOTS:
                binary = BUILD/f'fat_directory_preview.{section}.bin'
                run(['objcopy', '-O', 'binary', '-j', '.'+section, str(elf), str(binary)])
                body = binary.read_bytes()
                assert 0 < len(body) <= end-start
                candidate[start-0x800000:end-0x800000] = body+b'\x90'*(end-start-len(body))
                sizes[section] = dict(compiled_bytes=len(body), slot_bytes=end-start)
            if installed:
                production = (BUILD/'openicestaros_kernel.x').read_bytes()
                for _, _, start, end in SLOTS:
                    assert candidate[start-0x800000:end-0x800000] == production[start-0x800000:end-0x800000]
                candidate = bytearray(production)
            overlay = b''
        else:
            binary = BUILD/'fat_directory_ordinary.bin'
            run(['objcopy', '-O', 'binary', str(elf), str(binary)])
            overlay = binary.read_bytes()
        variants.append((label, bytes(candidate), overlay, [symbols[s] for _, s, _, _ in SLOTS]))
    return variants, sizes


class Machine:
    def __init__(self, label, kernel, overlay, entries):
        self.label, self.entries = label, entries
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        self.uc.mem_map(0, 0x1000000)
        self.uc.mem_write(0x800000, kernel)
        if overlay: self.uc.mem_write(0x300000, overlay)
        self.uc.hook_add(UC_HOOK_CODE, self.finish, begin=STOP, end=STOP)
        self.entry_indices = dict(zip(entries, range(len(entries))))
        for address in [*entries, 0x82a481]:
            self.uc.hook_add(UC_HOOK_CODE, self.observe, begin=address, end=address)

    def finish(self, uc, address, size, unused):
        self.done = True
        uc.emu_stop()

    def observe(self, uc, address, size, unused):
        sp = uc.reg_read(UC_X86_REG_ESP)
        if address == 0x82a481:
            left, right = struct.unpack('<2I', uc.mem_read(sp+4, 8))
            self.trace.append(['equal', bytes(uc.mem_read(left, 32)).split(b'\0')[0].hex(),
                               bytes(uc.mem_read(right, 128)).split(b'\0')[0].hex()])
            self.comparisons += 1
            if self.comparisons == 1:
                if 'attributes_during_compare' in self.case:
                    uc.mem_write(self.current_record+11, bytes([self.case['attributes_during_compare']]))
                if 'root_bound_during_compare' in self.case:
                    uc.mem_write(BPB+17, struct.pack('<H', self.case['root_bound_during_compare']))
                if self.case.get('geometry_during_compare'):
                    uc.mem_write(BPB+14, struct.pack('<H', 2))
                if self.case.get('cluster_bound_during_compare'):
                    uc.mem_write(BPB+13, b'\x02')
            return
        function = self.entry_indices[address]
        args = list(struct.unpack(f'<{ARGC[function]}I', uc.mem_read(sp+4, ARGC[function]*4)))
        if function in (0, 1):
            index = args[0 if function == 0 else 1]
            self.current_record = (self.root if function == 0 else args[0]) + ((index << 5) & 0xffffffff)
            if STACK-0x1000 <= args[-1] < STACK:
                args[-1] = 'local_name'
        self.trace.append([SLOTS[function][0], args])

    def execute(self, case):
        self.case, self.done, self.trace, self.comparisons = case, False, [], 0
        uc = self.uc
        for start, length, value in ((0x500000, 0x100000, 0), (0x700000, 0x20000, 0x96),
                                     (STACK-0x2000, 0x3000, 0)):
            uc.mem_write(start, bytes([value])*length)
        uc.mem_write(BPB, case['bpb'])
        bpb = case['bpb']
        self.root = BPB+(word(bpb, 14)+bpb[16]*word(bpb, 22))*word(bpb, 11)
        directory, index = case.get('directory_base', DIRECTORY), case.get('index', 0) & 0xffffffff
        uc.mem_write(self.root, case['root'])
        uc.mem_write(directory, case['directory'])
        if 'direct_record' in case:
            for base in (self.root, directory):
                uc.mem_write((base+(index << 5)) & 0xffffffff, case['direct_record'])
        if case.get('geometry_during_compare'):
            uc.mem_write(self.root+word(bpb, 11)+11, b'\x10')
        function = case['function']
        record = ((self.root if function % 2 == 0 else directory)+(index << 5)) & 0xffffffff
        output = record+case['alias'] if 'alias' in case else OUTPUT
        uc.mem_write(NAME, case.get('name', b'FILE.X')+b'\0')
        args = ([index, output], [directory, index, output], [index, output],
                [directory, index, output], [NAME], [directory, NAME], [index],
                [directory, index], [index], [directory, index], [index], [directory, index],
                [index], [directory, index], [NAME], [directory, NAME])[function]
        guard = [0x13579bdf, 0x2468ace0]
        uc.mem_write(STACK, struct.pack(f'<{len(args)+3}I', STOP, *args, *guard))
        saved = [(UC_X86_REG_EBX, 0x12345678), (UC_X86_REG_ESI, 0x87654321),
                 (UC_X86_REG_EDI, 0xabcdef12), (UC_X86_REG_EBP, 0x11223344)]
        for reg, value in [*saved, (UC_X86_REG_ESP, STACK), (UC_X86_REG_EFLAGS, 0x202),
                           (UC_X86_REG_ECX, 0x11112222), (UC_X86_REG_EDX, 0x33334444)]:
            uc.reg_write(reg, value)
        uc.emu_start(self.entries[function], STOP+1, count=1000000)
        assert self.done, (self.label, describe(case))
        assert uc.reg_read(UC_X86_REG_ESP) == STACK+4
        assert all(uc.reg_read(r) == v for r, v in saved)
        assert bytes(uc.mem_read(STACK+4, len(args)*4+8)) == struct.pack(f'<{len(args)+2}I', *args, *guard)
        result = uc.reg_read(UC_X86_REG_EAX)
        if 'expected_return' in case:
            assert result == case['expected_return'] & 0xffffffff, (self.label, describe(case), hex(result))
        if 'expected_output' in case:
            expected = case['expected_output']
            assert bytes(uc.mem_read(output, len(expected))) == expected, (self.label, describe(case))
            assert bytes(uc.mem_read(output+len(expected), 16)) == b'\x96'*16
        if function in (2, 3):
            assert result == output, (self.label, function, hex(result), hex(output), case.get('case'))
        return result, self.trace, bytes(uc.mem_read(0x500000, 0x100000)), bytes(uc.mem_read(0x700000, 0x20000))


def entry(name=b'FILE    X  ', attributes=0x20, size=1234, date=0, time=0):
    assert len(name) == 11
    value = bytearray(32)
    value[:11], value[11] = name, attributes
    struct.pack_into('<I', value, 28, size)
    struct.pack_into('<HH', value, 22, time, date)
    return bytes(value)


def fixture(function, *, records=None, bps=512, spc=1, entries=64, **options):
    bpb = bytearray(512)
    struct.pack_into('<HBHBHH', bpb, 11, bps, spc, 1, 2, entries, 0)
    struct.pack_into('<H', bpb, 22, 4)
    records = entry()+bytes(32) if records is None else records
    return dict(function=function, bpb=bytes(bpb), root=records,
                directory=options.pop('directory', records), **options)


def corpus(disk):
    directories = []
    def visit(data, path='/', seen=None):
        seen = set() if seen is None else seen
        directories.append((path, data))
        for offset in range(0, len(data), 32):
            record = data[offset:offset+32]
            if not record[0]: break
            if record[0] in (0xe5, 46) or record[11] & 8: continue
            if record[11] & 16:
                cluster = word(record, 26)
                assert cluster not in seen
                seen.add(cluster)
                raw = b''.join(disk.data[o:o+disk.cluster_bytes] for o in disk.chain(cluster))
                visit(raw, path+record[:8].rstrip().decode('ascii')+'/', seen)
    visit(disk.root)
    return directories


def cases(disk, source):
    samples = []
    records_checked = 0
    for path, data in corpus(disk):
        for offset in range(0, len(data), 32):
            record = data[offset:offset+32]
            if not record[0]: break
            records_checked += 1
            if record[0] == 0xe5 or record[11] == 0xf: continue
            name = record[:8].rstrip()+ (b'.'+record[8:11].rstrip() if record[8:11].strip() else b'')
            for function in range(16):
                if path != '/' and function % 2 == 0: continue
                index = offset//32
                options = dict(index=index, root=data if path == '/' else disk.root,
                    directory=data, bpb=source[17*512:18*512], function=function, name=name,
                    case=f'{path} {index} {function}')
                if function < 2:
                    options.update(expected_output=name+b'\0', expected_return=-3 if function == 0 and record[11]&8 else 1)
                elif function < 4: options['expected_output'] = record[8:11]+b'\0'
                elif function in (4, 5):
                    options['expected_return'] = -1 if record[11]&24 else index
                elif function in (6, 7): options['expected_return'] = struct.unpack_from('<I', record, 28)[0]
                elif function in (8, 9): options['expected_return'] = record[11]
                elif function < 14: options['expected_return'] = word(record, 24 if function < 12 else 22)
                else:
                    # The corpus has a volume label and a directory both named
                    # ICESTAR. Lookup must select the first eligible match,
                    # rather than infer the result from the input's record.
                    limit = word(options['bpb'], 17) if function == 14 else disk.cluster_bytes//32
                    matches = []
                    for candidate_index in range(min(limit, len(data)//32)):
                        candidate = data[candidate_index*32:(candidate_index+1)*32]
                        if not candidate[0]: break
                        if candidate[0] == 0xe5 or candidate[11] == 15: continue
                        if not candidate[11]&16 or (function == 14 and candidate[11]&8): continue
                        displayed = candidate[:8].rstrip() + (b'.'+candidate[8:11].rstrip()
                            if candidate[8:11].strip() else b'')
                        if displayed == name: matches.append(candidate_index)
                    options['expected_return'] = matches[0] if matches else -1
                samples.append(options)
            if not record[11]&24:
                for name_variant in (name.lower(), name+b'.BAD'):
                    samples.append(dict(function=4 if path == '/' else 5, root=data if path=='/' else disk.root,
                        directory=data, bpb=source[17*512:18*512], name=name_variant,
                        expected_return=-1 if name_variant != name else index))
    spellings = ((b'A          ', b'A'), (b'ABCDEFGHXYZ', b'ABCDEFGH.XYZ'),
        (b'ABCDEFG.XYZ', b'ABCDEFG.XYZ'), (b'A B     X  ', b'A.X'),
        (b'        X  ', b'.X'), (b'FILE     YZ', b'FILE'),
        (b'\x81ILE    X  ', b'\x81ILE.X'), (b'lower   x  ', b'lower.x'))
    for raw, displayed in spellings:
        for attributes in range(256):
            for function in (0, 1):
                status = -2 if attributes == 15 else -3 if function == 0 and attributes&8 else 1
                samples.append(fixture(function, records=entry(raw, attributes), expected_return=status,
                    **({'expected_output': displayed+b'\0'} if attributes != 15 else {})))
    for function in (0, 1):
        for marker, status in ((0, 0), (0xe5, -1)):
            samples.append(fixture(function, records=entry(bytes([marker])+b'ILE    X  '),
                                   expected_return=status, expected_output=b''))
    for function in (8, 9):
        for attributes in range(256):
            samples.append(fixture(function, records=entry(attributes=attributes), expected_return=attributes))
    for function in (6, 7):
        for size in (0, 1, 1234, 0x7fffffff, 0x80000000, 0xffffffff):
            samples.append(fixture(function, records=entry(size=size), expected_return=size))
    for function in (0, 1, 2, 3, 6, 7, 8, 9):
        for index in (0, 1, 7, -1, -0x80000000, 0x08000001):
            samples.append(fixture(function, index=index, direct_record=entry(size=0xffffffff)))
        for base in (DIRECTORY, DIRECTORY+3):
            for alias in (-2, 0, 1, 7, 8, 9, 10, 11, 12, 26):
                if function > 3: continue
                samples.append(fixture(function, directory_base=base, alias=alias))
    for function in (4, 5):
        for attributes in range(256):
            samples.append(fixture(function, records=entry(attributes=attributes)+bytes(32),
                expected_return=-1 if attributes&24 or attributes == 15 else 0))
        for prefix in (bytes(32), entry(b'\xe5ILE    X  '), entry(attributes=15), entry(b'OTHER   X  '),
                       entry(attributes=16), entry(attributes=8)):
            samples.append(fixture(function, records=prefix+entry()+bytes(32),
                expected_return=-1 if prefix[0] == 0 else 1))
        for index in (0, 1, 15, 16, 63, 64):
            samples.append(fixture(function, records=entry(b'OTHER   X  ')*index+entry()+bytes(32),
                expected_return=index if index < (64 if function == 4 else 16) else -1))
        for bound in (0, 1, 2):
            samples.append(fixture(function, records=entry(b'OTHER   X  ')+entry()+bytes(32),
                entries=bound, expected_return=1 if function == 5 or bound > 1 else -1))
        for attributes in (8, 16, 0, 0xff):
            samples.append(fixture(function, attributes_during_compare=attributes,
                expected_return=-1 if attributes&24 else 0))
        for bound in (0, 2):
            samples.append(fixture(function, records=entry(b'OTHER   X  ')+entry()+bytes(32),
                entries=1, root_bound_during_compare=bound,
                expected_return=1 if function == 5 or bound == 2 else -1))
        samples.append(fixture(function, geometry_during_compare=True,
                               expected_return=-1 if function == 4 else 0))
        samples.append(fixture(function, records=entry(b'OTHER   X  ')+entry()+bytes(32),
            bps=32, spc=1, cluster_bound_during_compare=True,
            expected_return=1 if function == 4 else -1))
    for bps, spc in ((0, 1), (512, 0), (128, 1), (1024, 16), (65535, 255)):
        samples.append(fixture(5, bps=bps, spc=spc, records=b'',
            directory=entry()+bytes(32), expected_return=-1 if not bps*spc else 0))
    for function in (10, 11, 12, 13):
        for value in (0, 1, 0x7fff, 0x8000, 0xfffe, 0xffff):
            for marker in (0, 0xe5, ord('F')):
                for attributes in (0, 15, 16, 255):
                    samples.append(fixture(function, records=entry(bytes([marker])+b'ILE    X  ',
                        attributes, date=value, time=value), expected_return=value))
        for index in (0, 1, 7, -1, -0x80000000, 0x08000001):
            for base in (DIRECTORY, DIRECTORY+3):
                samples.append(fixture(function, bps=511, directory_base=base, index=index,
                    direct_record=entry(date=0x1234, time=0xabcd),
                    expected_return=0x1234 if function < 12 else 0xabcd))
    for function in (14, 15):
        for attributes in range(256):
            accepts = bool(attributes&16) and not (function == 14 and attributes&8)
            samples.append(fixture(function, records=entry(attributes=attributes)+bytes(32),
                expected_return=0 if accepts else -1))
        for prefix in (bytes(32), entry(b'\xe5ILE    X  '), entry(attributes=15),
                       entry(b'OTHER   X  '), entry(attributes=0), entry(attributes=8)):
            samples.append(fixture(function, records=prefix+entry(attributes=16)+bytes(32),
                expected_return=-1 if prefix[0] == 0 else 1))
        for index in (0, 1, 15, 16, 63, 64):
            samples.append(fixture(function,
                records=entry(b'OTHER   X  ')*index+entry(attributes=16)+bytes(32),
                expected_return=index if index < (64 if function == 14 else 16) else -1))
        for attributes in (0, 8, 16, 24, 0xff):
            samples.append(fixture(function, records=entry(attributes=16)+bytes(32),
                attributes_during_compare=attributes, expected_return=0 if attributes&16 else -1))
        for bound in (0, 2):
            samples.append(fixture(function, records=entry(b'OTHER   X  ')+entry(attributes=16)+bytes(32),
                entries=1, root_bound_during_compare=bound,
                expected_return=1 if function == 15 or bound == 2 else -1))
        for name in (b'FILE.X', b'file.x', b'FILE.X.BAD', b''):
            samples.append(fixture(function, records=entry(attributes=16)+bytes(32), name=name,
                expected_return=0 if name == b'FILE.X' else -1))
        samples.append(fixture(function, records=entry(attributes=16)+bytes(32),
            geometry_during_compare=True, expected_return=0))
        samples.append(fixture(function, records=entry(b'OTHER   X  ')+entry(attributes=16)+bytes(32),
            bps=32, spc=1, cluster_bound_during_compare=True,
            expected_return=1 if function == 14 else -1))
    return samples, records_checked


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installed', action='store_true')
    arguments = parser.parse_args()
    started = time.monotonic()
    source = (ROOT.parent/'IceStarOS_x86_32M.vhd').read_bytes()
    disk = Fat16(source); original = disk.read_kernel()
    assert hashlib.sha256(original).hexdigest() == ORIGINAL_SHA256
    variants, sizes = compile_variants(original, arguments.installed)
    machines = [Machine('original', original, b'', [s[2] for s in SLOTS])]+[Machine(*v) for v in variants]
    samples, records_checked = cases(disk, source)
    for index, case in enumerate(samples):
        expected = machines[0].execute(case)
        for machine in machines[1:]:
            actual = machine.execute(case)
            assert actual == expected, (index, machine.label, describe(case),
                [(i, a[:16] if isinstance(a, bytes) else a, b[:16] if isinstance(b, bytes) else b)
                 for i, (a, b) in enumerate(zip(expected, actual)) if a != b])
    summary = dict(installed=arguments.installed, scenarios=len(samples), executions=len(samples)*3,
        corpus_directories=len(corpus(disk)), corpus_records=records_checked, slots=sizes,
        seconds=round(time.monotonic()-started, 3), boundaries='No callbacks or stubbed services',
        scope='Return values, complete checked memory, ordered service calls, cdecl ABI; bounded record/output aliases',
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest()
                for p in ('kernel/fat_directory.c', 'kernel/fat_directory.h', 'tests/kernel_fat_directory.py')})
    (BUILD/'fat_directory_summary.json').write_text(json.dumps(summary, indent=2)+'\n')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__': main()
