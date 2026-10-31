"""Compare eight real contiguous/directory readers with original x86.

Only the allocator and ATA single-sector hardware boundary are intercepted.
All reader-to-reader calls execute, including the installed production image.
"""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'
sys.path[:0] = [str(ROOT), str(BUILD / 'test-deps')]
from tools.kernel_image import Fat16, ORIGINAL_SHA256, word
from tests.kernel_x_loader import FLAGS, run
from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_MEM_WRITE, UC_ERR_EXCEPTION
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
    UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP,
    UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EFLAGS)

SLOTS = [
    ('x_lba_alloc', 'kernel_x_read_lba_alloc_8252b4', 0x8252b4, 0x82539e),
    ('x_lba_into', 'kernel_x_read_lba_into_82539e', 0x82539e, 0x825476),
    ('fat_sectors_alloc', 'kernel_fat_read_sectors_alloc_825476', 0x825476, 0x8254ca),
    ('fat_read_sectors', 'kernel_fat_read_sectors_8254ca', 0x8254ca, 0x825506),
    ('fat_read_partition', 'kernel_fat_read_partition_825506', 0x825506, 0x8255ae),
    ('fat_read_root_directory', 'kernel_fat_read_root_directory_8255ae', 0x8255ae, 0x8256a8),
    ('fat_alloc_root_directory', 'kernel_fat_alloc_root_directory_8256a8', 0x8256a8, 0x8257bc),
    ('fat_alloc_directory', 'kernel_fat_alloc_directory_8257bc', 0x8257bc, 0x8258a6),
]
BASE, BPB, DIRECTORY, ARENA, DESTINATION = 0x800000, 0x520000, 0x600000, 0x900000, 0x980000
STACK, STOP, MASK = 0x40f000, 0x200000, 0xffffffff
SAVED = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
SENTINELS = (0x11223344, 0x55667788, 0x19283746, 0x31415926)


def compile_variants(original, installed):
    obj = BUILD / 'kernel_fat_block_preview.o'
    run(['gcc', *FLAGS, '-c', 'kernel/fat_block.c', '-o', str(obj)])
    outputs, sizes = [], {}
    for fixed in (False, True):
        label = 'fixed' if fixed else 'ordinary'
        script, elf = BUILD/f'fat_block_{label}.ld', BUILD/f'fat_block_{label}.elf'
        sections = '\n'.join(
            f'.{name} {start:#x} : {{ *(.text.{symbol}) }}\n'
            f'ASSERT({symbol} == {start:#x} && SIZEOF(.{name}) <= {end-start}, "slot overflow")'
            for name, symbol, start, end in SLOTS) if fixed else '. = 0x300000; .text : { *(.text*) }'
        script.write_text('OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_x_read_lba_alloc_8252b4)\n'
            'PROVIDE(kernel_allocate = 0x815464);\n'
            'PROVIDE(kernel_fat_read_sector_828b44 = 0x828b44);\nSECTIONS {\n' + sections +
            '\n/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n'
            '.unexpected : { *(*) }\nASSERT(SIZEOF(.unexpected)==0, "unexpected section")\n}\n')
        run(['ld', '-m', 'elf_i386', '-T', str(script), '-o', str(elf), str(obj)])
        symbols = {line.split()[-1]: int(line.split()[0], 16)
            for line in run(['nm', str(elf)], capture_output=True, text=True).stdout.splitlines()}
        if fixed:
            candidate = bytearray(original)
            for name, symbol, start, end in SLOTS:
                path = BUILD/f'fat_block_preview.{name}.bin'
                run(['objcopy', '-O', 'binary', '-j', '.'+name, str(elf), str(path)])
                body = path.read_bytes()
                assert 0 < len(body) <= end-start
                candidate[start-BASE:end-BASE] = body + b'\x90'*(end-start-len(body))
                sizes[name] = dict(compiled_bytes=len(body), slot_bytes=end-start)
            if installed:
                production = (BUILD/'openicestaros_kernel.x').read_bytes()
                for _, _, start, end in SLOTS:
                    assert production[start-BASE:end-BASE] == candidate[start-BASE:end-BASE]
                candidate = bytearray(production)
            outputs.append((label, bytes(candidate), BASE, [symbols[s] for _,s,_,_ in SLOTS]))
        else:
            path = BUILD/'fat_block_ordinary.bin'
            run(['objcopy', '-O', 'binary', str(elf), str(path)])
            outputs.append((label, path.read_bytes(), 0x300000, [symbols[s] for _,s,_,_ in SLOTS]))
    return outputs, sizes


class Machine:
    def __init__(self, label, binary, base, entries, original, source):
        self.label, self.entries, self.source = label, entries, source
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        self.uc.mem_map(0, 0x1000000)
        self.uc.mem_map(0xffff0000, 0x10000)
        self.uc.mem_write(BASE, original)
        self.uc.mem_write(base, binary)
        self.uc.hook_add(UC_HOOK_CODE, self.finish, begin=STOP, end=STOP)
        for address in (0x815464, 0x828b44):
            self.uc.hook_add(UC_HOOK_CODE, self.boundary, begin=address, end=address)
        self.uc.hook_add(UC_HOOK_MEM_WRITE, self.written)
        self.old_pages = set()

    def finish(self, uc, address, size, unused):
        self.done = True
        uc.emu_stop()

    def written(self, uc, access, address, size, value, unused):
        if not 0x400000 <= address < 0x410000:
            self.mark(address, size)

    def mark(self, address, size):
        while size:
            self.pages.add(address & ~4095)
            step = min(size, 4096-(address & 4095))
            address, size = (address+step)&MASK, size-step

    def put(self, address, data):
        end = min(len(data), 0x100000000-address)
        self.uc.mem_write(address, data[:end])
        if end < len(data):
            self.uc.mem_write(0, data[end:])
        self.mark(address, len(data))

    def boundary(self, uc, address, size, unused):
        sp = uc.reg_read(UC_X86_REG_ESP)
        argc = 1 if address == 0x815464 else 2
        args = struct.unpack(f'<{argc}I', uc.mem_read(sp+4, argc*4))
        if address == 0x815464:
            result = self.case.get('allocation', ARENA)
            self.trace.append(('allocate', args, result))
            mutation = self.case.get('allocate_mutation')
            if mutation:
                for target, data in mutation:
                    self.put(target, data)
        else:
            lba, destination = args
            assert len(self.reads) < 2048, 'unbounded read'
            payload = self.case.get('sectors', {}).get(lba)
            if payload is None:
                offset = lba*512
                payload = (self.source[offset:offset+512] if self.case.get('corpus')
                    and offset+512 <= len(self.source) else bytes((lba*17+i*29)&255 for i in range(512)))
            assert len(payload) == 512
            result = self.case.get('read_status', 0xdeadbeef)
            self.trace.append(('read', args, hashlib.sha256(payload).hexdigest(), result))
            self.reads.append((lba, destination, payload))
            if not self.case.get('no_transfer'):
                self.put(destination, payload)
            if len(self.reads) == self.case.get('mutate_read'):
                for target, data in self.case['read_mutation']:
                    self.put(target, data)
        uc.reg_write(UC_X86_REG_EAX, result)
        uc.reg_write(UC_X86_REG_ECX, 0x88776655)
        uc.reg_write(UC_X86_REG_EDX, 0xabcdef01)
        uc.reg_write(UC_X86_REG_ESP, sp+4)
        uc.reg_write(UC_X86_REG_EIP, struct.unpack('<I', uc.mem_read(sp,4))[0])

    def execute(self, case):
        uc = self.uc
        for page in self.old_pages:
            uc.mem_write(page, bytes(4096))
        self.pages, self.trace, self.reads, self.done, self.case = set(), [], [], False, case
        for address, data in case.get('setup', []):
            self.put(address, data)
        arguments = case['args']
        stack = struct.pack('<'+'I'*(len(arguments)+1), STOP, *arguments)
        uc.mem_write(STACK-0x2000, bytes(0x3000))
        uc.mem_write(STACK, stack)
        uc.reg_write(UC_X86_REG_ESP, STACK)
        uc.reg_write(UC_X86_REG_EFLAGS, 2)
        for reg, value in zip(SAVED, SENTINELS):
            uc.reg_write(reg, value)
        fault = False
        try:
            uc.emu_start(self.entries[case['function']], 0, count=3000000)
        except UcError as error:
            assert case.get('fault') and error.errno == UC_ERR_EXCEPTION, (self.label, case, error)
            fault = True
        assert fault == case.get('fault', False) and (fault or self.done), (self.label, case)
        result = uc.reg_read(UC_X86_REG_EAX) if not fault else 'divide fault'
        if not fault:
            assert uc.reg_read(UC_X86_REG_ESP) == STACK+4
            assert bytes(uc.mem_read(STACK+4, len(arguments)*4)) == stack[4:], 'caller arguments overwritten'
            assert tuple(uc.reg_read(reg) for reg in SAVED) == SENTINELS
        if 'expected_return' in case:
            assert result == case['expected_return'], (self.label, case, result)
        if 'expected_reads' in case:
            assert len(self.reads) == case['expected_reads'], (self.label, case, len(self.reads))
        if 'expected_payload' in case:
            actual = b''.join(payload for _,_,payload in self.reads[case.get('skip_header', 0):])
            assert actual == case['expected_payload']
        if 'expected_allocation' in case:
            allocations = [entry[1][0] for entry in self.trace if entry[0] == 'allocate']
            assert allocations == [case['expected_allocation']]
        memory = [(page, hashlib.sha256(uc.mem_read(page,4096)).hexdigest()) for page in sorted(self.pages)]
        self.old_pages = self.pages
        return result, self.trace, memory


def geometry(bps=512, spc=2, reserved=1, fats=2, fat_sectors=4, entries=32):
    data = bytearray(512)
    struct.pack_into('<H', data, 11, bps)
    data[13], data[16] = spc, fats
    struct.pack_into('<H', data, 14, reserved)
    struct.pack_into('<H', data, 17, entries)
    struct.pack_into('<H', data, 22, fat_sectors)
    return bytes(data)


def directory_case(function, *, bps=512, spc=2, attributes=0x10,
                   cluster=6, high=0, index=3, **kwargs):
    bpb = geometry(bps=bps, spc=spc)
    record = bytearray(32)
    record[0], record[11] = 0xe5, attributes
    struct.pack_into('<HH', record, 20, high, 0)
    struct.pack_into('<H', record, 26, cluster)
    address = ((DIRECTORY if function == 7 else BPB+9*bps)+(index*32))&MASK
    args = [index&MASK, DESTINATION] if function == 5 else ([index&MASK] if function == 6 else [index&MASK,DIRECTORY])
    return dict(function=function,args=args,setup=[(BPB,bpb),(0x7dc6,struct.pack('<I',17)),
        (address,bytes(record))],**kwargs)


def scenarios(source):
    cases = []
    sizes = [0,1,511,512,513,1023,1024,1025,8193,65536,-1,-511,-512,-513,-1025,-2147483648]
    for function in (0,1):
        for size in sizes:
            count = abs(size)//512*(1 if size >= 0 else -1)+(1 if size > 0 and size%512 else 0)
            header = bytearray(512)
            struct.pack_into('<i',header,4,size)
            for allocation in (ARENA,0,0xfffffe00):
                cases.append(dict(function=function,args=[100] if function == 0 else [100,allocation],
                    sectors={100:bytes(header)},allocation=allocation,
                    expected_return=allocation,expected_reads=1+max(count,0),
                    **(dict(expected_allocation=(count<<9)&MASK) if function == 0 else {})))
    for function in (2,3):
        for count in (0,1,2,3,16,32,0x80000000,0xfffffffe,0xffffffff):
            for allocation in (ARENA,0,0x80000000,0xfffffe00):
                for lba in (17,0xffffffff):
                    # Negative allocation is returned without dereferencing by
                    # the allocating API; the supplied-buffer API may wrap.
                    if function == 3 and allocation == 0x80000000 and 0<count<0x80000000:
                        continue
                    read_count = max(count if count<0x80000000 else count-0x100000000,0)
                    if function == 2 and (allocation == 0 or allocation>=0x80000000): read_count=0
                    cases.append(dict(function=function,args=([lba,count] if function==2 else [lba,count,allocation]),
                        allocation=allocation,expected_return=allocation,expected_reads=read_count,
                        **(dict(expected_allocation=(count<<9)&MASK) if function == 2 else {})))
    for function in (5,6,7):
        for bps,spc in ((512,2),(256,3),(1024,1),(513,3),(1,0),(0,0),(0,1)):
            for attr in (0,0x08,0x10,0xff):
                for index in ((3,4,0x80000003) if bps<4 else (0,3,0x80000003)):
                    # Tiny BPBs can overlap a root record; use a buffer index
                    # that avoids the BPB fields for the zero-byte fault probes.
                    case=directory_case(function,bps=bps,spc=spc,attributes=attr,index=index)
                    case['fault'] = bool(attr&16 and bps==0)
                    if not attr&16:
                        case.update(expected_return=0,expected_reads=0)
                    elif bps:
                        case['expected_reads']=spc
                        case['expected_return']=1 if function==5 else ARENA
                    cases.append(case)
        for cluster in (0,1,2,0xfff8,0xffff):
            for high in (0,1,0xffff):
                cases.append(directory_case(function,cluster=cluster,high=high))
        for allocation in (0,0xfffffe00):
            cases.append(directory_case(function,allocation=allocation,expected_reads=2,
                expected_return=1 if function==5 else allocation))
        if function != 5:
            # Allocation changes BPB, record attributes/cluster and partition
            # LBA: record address was cached, data LBA and spc are live.
            record_address=(DIRECTORY if function==7 else BPB+9*512)+3*32
            cases.append(directory_case(function,allocate_mutation=[
                (BPB,geometry(bps=256,spc=3)),(0x7dc6,struct.pack('<I',0xfffffffe)),
                (record_address+11,b'\0'),(record_address+26,struct.pack('<H',9))],
                expected_allocation=1024,expected_reads=3,expected_return=ARENA))
            cases.append(directory_case(function,allocate_mutation=[(BPB+11,b'\0\0')],fault=True))
    for bps in (0,1,256,512,513,1024):
        for index in (0,1,3,0x10000000,0xffffffff):
            bpb=geometry(bps=bps)
            partition=(0x7dbe+(index<<4))&MASK
            for destination in (DESTINATION,0x7dbe):
                cases.append(dict(function=4,args=[index,destination],setup=[(partition+8,struct.pack('<I',17))],
                    sectors={17:bpb},fault=bps==0,
                    **(dict(expected_return=(destination+bps)&MASK) if bps else {})))
    # Change the partition LBA after the boot-sector read: the metadata
    # start must reread it. Change geometry during the first cluster sector:
    # the already selected count/LBA and fixed 512-byte stride must persist.
    cases.append(dict(function=4,args=[0,DESTINATION],
        setup=[(0x7dc6,struct.pack('<I',17))],sectors={17:geometry()},
        mutate_read=1,read_mutation=[(0x7dc6,struct.pack('<I',0xfffffffe))],
        expected_return=DESTINATION+512,expected_reads=11))
    for function in (5,6,7):
        cases.append(directory_case(function,mutate_read=1,
            read_mutation=[(BPB,geometry(bps=1024,spc=0)),
                           (0x7dc6,struct.pack('<I',0xffffffff))],expected_reads=2))
    # Status codes do not alter the reader loops, and reads can alias live
    # metadata. The contiguous reader never consults the BPB for its stride.
    for function in range(8):
        if function>=5: case=directory_case(function)
        elif function==4:
            case=dict(function=4,args=[0,DESTINATION],setup=[(0x7dc6,struct.pack('<I',17))],sectors={17:geometry()})
        else:
            case=dict(function=function,args=([100] if function==0 else [100,DESTINATION] if function==1
                else [100,3] if function==2 else [100,3,DESTINATION]),sectors={100:geometry()})
        case.update(read_status=0,no_transfer=True)
        if function in (0,1): case.setdefault('setup',[]).append((0x18a04,struct.pack('<I',513)))
        if function==4: case.setdefault('setup',[]).append((DESTINATION,geometry()))
        cases.append(case)
    for destination in (BPB,0x7dc6,0x18a00,0xfffffe00):
        cases.append(dict(function=3,args=[0xffffffff,3,destination],expected_return=destination,
            expected_reads=3,setup=[(BPB,geometry(bps=256))]))
    # Real boot BPB, metadata, X headers and root/subdirectory data; contiguous
    # executable reading intentionally does not follow fragmented FAT chains.
    disk=Fat16(source)
    inventory=json.loads((ROOT/'recovery/inventory.json').read_text())
    for program in inventory['programs']:
        contents=disk.root
        names=[name.encode('ascii') for name in program['short_names']]
        for i,name in enumerate(names):
            record=disk.find(contents,name,i+1<len(names))
            if i+1<len(names): contents=b''.join(source[o:o+disk.cluster_bytes] for o in disk.chain(word(record,26)))
        lba=next(iter(disk.chain(word(record,26))))//512
        size=struct.unpack_from('<i',source,lba*512+4)[0]
        count=(size+511)//512
        for function in (0,1,2,3):
            args=([lba] if function==0 else [lba,DESTINATION] if function==1 else [lba,count]
                if function==2 else [lba,count,DESTINATION])
            cases.append(dict(function=function,args=args,corpus=True,name=program['path'],
                expected_reads=count+int(function<2),skip_header=int(function<2),
                expected_payload=source[lba*512:(lba+count)*512],
                expected_return=ARENA if function in (0,2) else DESTINATION))
    cases.append(dict(function=4,args=[0,DESTINATION],corpus=True,
        setup=[(0x7dc6,struct.pack('<I',17))],expected_return=DESTINATION+4096,
        expected_payload=source[17*512:18*512]+source[25*512:89*512],expected_reads=65))
    for index in range(len(disk.root)//32):
        record=disk.root[index*32:(index+1)*32]
        if not record[0]: break
        for function in (5,6,7):
            args=[index,DESTINATION] if function==5 else [index] if function==6 else [index,DIRECTORY]
            cases.append(dict(function=function,args=args,corpus=True,
                setup=[(BPB,source[17*512:89*512]),(0x7dc6,struct.pack('<I',17)),(DIRECTORY,disk.root)],
                expected_reads=16 if record[11]&16 else 0,
                expected_return=(1 if function==5 else ARENA) if record[11]&16 else 0))
    # Deterministic independent geometry/count samples.
    rng=random.Random(0x8252b4)
    for _ in range(100):
        function=rng.choice((2,3,5,6,7))
        if function in (2,3):
            count=rng.randrange(65);lba=rng.getrandbits(32)
            cases.append(dict(function=function,args=[lba,count] if function==2 else [lba,count,DESTINATION],
                expected_reads=count,expected_return=ARENA if function==2 else DESTINATION))
        else:
            cases.append(directory_case(function,bps=rng.choice((128,256,512,1024)),spc=rng.randrange(17),
                cluster=rng.randrange(65536),high=rng.randrange(65536)))
    return cases


def main():
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--installed',action='store_true')
    args=parser.parse_args()
    started=time.monotonic()
    source=(ROOT.parent/'IceStarOS_x86_32M.vhd').read_bytes()
    original=Fat16(source).read_kernel()
    assert hashlib.sha256(original).hexdigest()==ORIGINAL_SHA256
    variants,sizes=compile_variants(original,args.installed)
    specs=[('original',original,BASE,[s[2] for s in SLOTS]),*variants]
    machines=[Machine(*s,original,source) for s in specs]
    cases=scenarios(source)
    for index,case in enumerate(cases):
        current=[Machine(*s,original,source) for s in specs] if case.get('fault') else machines
        try:
            expected=current[0].execute(case)
            for machine in current[1:]:
                actual=machine.execute(case)
                assert actual==expected,(machine.label,expected[:2],actual[:2],expected[2],actual[2])
        except Exception:
            print('Failing case',index,{k:v for k,v in case.items() if k not in ('setup','sectors','expected_payload')},flush=True)
            raise
    summary=dict(installed=args.installed,scenarios=len(cases),executions=len(cases)*3,
        slots=sizes,seconds=round(time.monotonic()-started,3),corpus_paths=39,
        boundary_scope='Only allocation and ATA single-sector read; nested contiguous readers execute',
        checks=['ordered boundary calls and payloads','all touched data pages','return values',
                'callee-saved registers','caller argument stack','signed counts','divide faults'],
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in
                ('kernel/fat_block.c','kernel/fat_block.h','tests/kernel_fat_block.py')})
    (BUILD/'fat_block_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2))


if __name__=='__main__':
    main()
