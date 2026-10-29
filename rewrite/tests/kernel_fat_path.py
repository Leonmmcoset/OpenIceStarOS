"""Execute original and C path lookup with real FAT traversal and directory data.

Only allocation/free and hardware sector I/O are intercepted. Parsing, 8.3
formatting, uppercase conversion, directory reads and record copies execute
as x86. No pathname or directory-lookup callback supplies successful results.
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
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ESI,
    UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP, UC_X86_REG_EIP,
    UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EFLAGS)

SLOTS = [('fat_find_path', 'kernel_x_find_path_8263c0', 0x8263c0, 0x826940),
         ('fat_name83', 'kernel_fat_name83_827bba', 0x827bba, 0x827c72),
         ('fat_find_directory_path', 'kernel_fat_find_directory_path_825f48', 0x825f48, 0x8263c0)]
EXTERNAL = {'kernel_string_upper_82a531': 0x82a531,
    'kernel_character_upper_82a755': 0x82a755, 'kernel_rep_copy_bytes': 0x817484,
    'kernel_fat_read_alloc_827454': 0x827454, 'kernel_free_allocation': 0x815978}
BPB, FAT, NAME, OUTPUT, ARENA, STACK, STOP = 0x520000, 0x500000, 0x700100, 0x710100, 0x900000, 0x40f000, 0x200000


def compile_variants(original, installed):
    objects = []
    for module in ('fat_path', 'fat_directory_path'):
        obj = BUILD / f'kernel_{module}_preview.o'
        run(['gcc', *FLAGS, '-c', f'kernel/{module}.c', '-o', str(obj)])
        objects.append(str(obj))
    variants, sizes = [], {}
    for fixed in (False, True):
        label = 'fixed' if fixed else 'ordinary'
        script, elf = BUILD/f'fat_path_{label}.ld', BUILD/f'fat_path_{label}.elf'
        sections = '\n'.join(
            f'.{section} {start:#x} : {{ *(.text.{symbol}) }}\n'
            f'ASSERT({symbol}=={start:#x} && SIZEOF(.{section})<={end-start}, "slot overflow")'
            for section, symbol, start, end in SLOTS) if fixed else '. = 0x300000; .text : { *(.text*) }'
        script.write_text('OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_x_find_path_8263c0)\n'+
            '\n'.join(f'PROVIDE({name} = {address:#x});' for name,address in EXTERNAL.items())+
            '\nSECTIONS {\n'+sections+'\n/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n'+
            '.unexpected : { *(*) }\nASSERT(SIZEOF(.unexpected)==0,"unexpected section")\n}\n')
        run(['ld', '-m', 'elf_i386', '-T', str(script), '-o', str(elf), *objects])
        symbols = {line.split()[-1]: int(line.split()[0],16)
                   for line in run(['nm', str(elf)],capture_output=True,text=True).stdout.splitlines()}
        candidate = bytearray(original)
        if fixed:
            for section, _, start, end in SLOTS:
                binary = BUILD/f'fat_path_preview.{section}.bin'
                run(['objcopy','-O','binary','-j','.'+section,str(elf),str(binary)])
                body = binary.read_bytes()
                assert 0 < len(body) <= end-start
                candidate[start-0x800000:end-0x800000] = body+b'\x90'*(end-start-len(body))
                sizes[section] = dict(compiled_bytes=len(body), slot_bytes=end-start)
            if installed:
                production = (BUILD/'openicestaros_kernel.x').read_bytes()
                for _,_,start,end in SLOTS:
                    assert candidate[start-0x800000:end-0x800000] == production[start-0x800000:end-0x800000]
                candidate = bytearray(production)
            overlay, base = b'', 0
        else:
            binary = BUILD/'fat_path_ordinary.bin'
            run(['objcopy','-O','binary',str(elf),str(binary)])
            overlay, base = binary.read_bytes(), 0x300000
        variants.append((label,bytes(candidate),overlay,base,[symbols[s] for _,s,_,_ in SLOTS]))
    return variants, sizes


class Machine:
    def __init__(self,label,kernel,overlay,base,entries):
        self.label,self.entries = label,entries
        self.uc = Uc(UC_ARCH_X86,UC_MODE_32)
        self.uc.mem_map(0,0x1000000)
        self.uc.mem_write(0x800000,kernel)
        if overlay: self.uc.mem_write(base,overlay)
        self.uc.hook_add(UC_HOOK_CODE,self.finish,begin=STOP,end=STOP)
        for address in (0x815464,0x815978,0x8254ca):
            self.uc.hook_add(UC_HOOK_CODE,self.boundary,begin=address,end=address)

    def finish(self,uc,address,size,unused):
        self.done = True
        uc.emu_stop()

    def boundary(self,uc,address,size,unused):
        sp = uc.reg_read(UC_X86_REG_ESP)
        argc = 3 if address == 0x8254ca else 1
        args = list(struct.unpack(f'<{argc}I',uc.mem_read(sp+4,argc*4)))
        result = 0x7351
        if address == 0x815464:
            self.allocations += 1
            result = ARENA+(self.allocations-1)*0x10000
            assert self.allocations <= 32
            if self.allocations == self.case.get('fail_allocation'): result = 0
            self.trace.append(['allocate',args,result])
        elif address == 0x815978:
            self.trace.append(['free',args])
            if self.case.get('free_mutation'):
                uc.mem_write(args[0]+26,struct.pack('<H',7))
        else:
            lba,count,destination = args
            assert count <= 16 and ARENA <= destination <= ARENA+0x200000-count*512,args
            if 'disk' in self.case:
                payload = self.case['disk'][lba*512:(lba+count)*512]
            else:
                payload = b''.join(self.case['sectors'].get(lba+i,bytes(512)) for i in range(count))
            assert len(payload) == count*512
            uc.mem_write(destination,payload)
            self.trace.append(['read',args,hashlib.sha256(payload).hexdigest()])
        uc.reg_write(UC_X86_REG_EAX,result)
        uc.reg_write(UC_X86_REG_ECX,0x11112222)
        uc.reg_write(UC_X86_REG_EDX,0x33334444)
        uc.reg_write(UC_X86_REG_ESP,sp+4)
        uc.reg_write(UC_X86_REG_EIP,struct.unpack('<I',uc.mem_read(sp,4))[0])

    def execute(self,case):
        uc = self.uc
        self.case,self.done,self.trace,self.allocations = case,False,[],0
        for start,length,value in ((0,0x20000,0),(0x500000,0x80000,0),
                                   (0x700000,0x20000,0x96),(ARENA,0x200000,0xa5),
                                   (STACK-0x4000,0x5000,0)):
            uc.mem_write(start,bytes([value])*length)
        uc.mem_write(BPB,case['bpb'])
        uc.mem_write(FAT,case['fat'])
        uc.mem_write(0x1a83a,struct.pack('<I',FAT))
        uc.mem_write(0x7dc6,struct.pack('<I',17))
        bpb = case['bpb']
        root = BPB+(word(bpb,14)+bpb[16]*word(bpb,22))*word(bpb,11)
        uc.mem_write(root,case['root'])
        uc.mem_write(NAME,case['name']+b'\0')
        output = NAME if case.get('in_place') else OUTPUT
        args = [NAME,output]
        guard = [0x13579bdf,0x2468ace0]
        uc.mem_write(STACK,struct.pack('<5I',STOP,*args,*guard))
        saved = [(UC_X86_REG_EBX,0x12345678),(UC_X86_REG_ESI,0x87654321),
                 (UC_X86_REG_EDI,0xabcdef12),(UC_X86_REG_EBP,0x11223344)]
        for reg,value in [*saved,(UC_X86_REG_ESP,STACK),(UC_X86_REG_EFLAGS,0x202)]: uc.reg_write(reg,value)
        uc.emu_start(self.entries[case.get('function',0)],STOP+1,count=1000000)
        assert self.done,(self.label,case['name'],hex(uc.reg_read(UC_X86_REG_EIP)))
        assert uc.reg_read(UC_X86_REG_ESP)==STACK+4
        assert all(uc.reg_read(r)==v for r,v in saved)
        assert bytes(uc.mem_read(STACK+4,16))==struct.pack('<4I',*args,*guard)
        result = uc.reg_read(UC_X86_REG_EAX)
        if 'expected_return' in case: assert result == case['expected_return']&0xffffffff,(case['name'],result)
        if 'expected_record' in case: assert bytes(uc.mem_read(output,32))==case['expected_record']
        if 'expected_name83' in case:
            assert bytes(uc.mem_read(output,12))==case['expected_name83'],case['name']
            assert result==output+11
        if 'expected_allocations' in case: assert self.allocations==case['expected_allocations'],self.trace
        if 'expected_frees' in case: assert sum(t[0]=='free' for t in self.trace)==case['expected_frees'],self.trace
        return (result,self.trace,bytes(uc.mem_read(0,0x20000)),bytes(uc.mem_read(0x500000,0x80000)),
                bytes(uc.mem_read(0x700000,0x20000)),bytes(uc.mem_read(ARENA,0x200000)))


def record(name,attributes=0x20,cluster=5,size=7):
    value = bytearray(32)
    value[:11] = name
    value[11] = attributes
    struct.pack_into('<H',value,26,cluster)
    struct.pack_into('<I',value,28,size)
    return bytes(value)


def synthetic(name=b'C:\\A\\FILE.X',*,root=None,directory=None,**options):
    bpb = bytearray(512)
    struct.pack_into('<HBHBHH',bpb,11,512,1,1,2,64,0)
    struct.pack_into('<H',bpb,22,4)
    fat = bytearray(0x20000)
    for cluster in (5,7): struct.pack_into('<H',fat,cluster*2,0xffff)
    root = root if root is not None else record(b'A          ',0x10,5,0)+bytes(32)
    directory = directory if directory is not None else record(b'FILE    X  ')+bytes(32)
    return dict(name=name,bpb=bytes(bpb),fat=bytes(fat),root=root,
                sectors={33:directory.ljust(512,b'\0')},**options)


def walk(disk,contents,prefix=(),seen=None):
    seen = set() if seen is None else seen
    for offset in range(0,len(contents),32):
        entry=contents[offset:offset+32]
        if not entry[0]: break
        if entry[0]==0xe5 or entry[11]&8 or entry[:1]==b'.': continue
        name=entry[:8].rstrip()+ (b'.'+entry[8:11].rstrip() if entry[8:11].strip() else b'')
        path=(*prefix,name)
        if entry[11]&16:
            cluster=word(entry,26)
            if cluster not in seen:
                seen.add(cluster)
                data=b''.join(disk.data[o:o+disk.cluster_bytes] for o in disk.chain(cluster))
                yield from walk(disk,data,path,seen)
        else: yield b'C:\\'+b'\\'.join(path),entry


def cases(disk,source):
    samples=[]
    for path,entry in walk(disk,disk.root):
        for name in (path,path.lower(),path+b'\\'):
            samples.append(dict(name=name,bpb=source[17*512:18*512],fat=disk.fat,
                root=disk.root,disk=source,expected_return=1,expected_record=entry))
        samples.append(dict(name=path+b'.BAD',bpb=source[17*512:18*512],fat=disk.fat,
                           root=disk.root,disk=source))
    malformed=(b'',b'\\',b'\\A',b'C:\\\\FILE.X',b'C:\\A\\\\FILE.X',b'A',b'C:',b'D:\\A\\FILE.X',
        b'C:/A/FILE.X',b'C:\\ABCDEFGHIJKLM',b'C:\\A.1234567890',b'C:\\A\\FILE.X\\\\',b'C:\\'+b'A\\'*32+b'FILE.X')
    for name in malformed: samples.append(synthetic(name))
    for name in (b'C:\\A\\FILE.X',b'c:\\a\\file.x',b'C:EXTRA\\A\\FILE.X'):
        samples.append(synthetic(name,expected_return=1,expected_record=record(b'FILE    X  '),expected_allocations=1,expected_frees=0))
    for attribute in (0,1,8,16,0x18,0x20,0xff):
        samples.append(synthetic(directory=record(b'FILE    X  ',attribute),expected_return=0 if attribute&24 else 1))
        samples.append(synthetic(root=record(b'A          ',attribute,5),expected_return=1 if attribute&16 else 0))
    for prefix in (bytes(32),record(b'OTHER      '),record(b'\xe5ILE    X  '),record(b'FILE    X  ',8)):
        samples.append(synthetic(directory=prefix+record(b'FILE    X  '),expected_return=0 if prefix[0]==0 else 1))
    for fail in (1,2): samples.append(synthetic(fail_allocation=fail))
    # A reused child directory provides bounded deep traversal without faking
    # lookup results. The original frees before consuming the found record.
    for depth in (1,2,3,10,30,31,32,33):
        directory=record(b'A          ',0x10,5,0)+record(b'FILE    X  ')+bytes(32)
        for suffix in (b'FILE.X',b'MISSING.X'):
            loaded=min(depth,30)
            samples.append(synthetic(b'C:\\'+b'A\\'*depth+suffix,directory=directory,
                expected_return=int(depth<=30 and suffix==b'FILE.X'),
                expected_allocations=loaded,expected_frees=loaded if loaded>1 else 0))
        samples.append(synthetic(b'C:\\'+b'A\\'*depth+b'FILE.X',directory=directory,free_mutation=True))
    # Signed-vs-unsigned name-byte comparisons reject high-bit short names.
    samples.append(synthetic(b'C:\\\x81\\FILE.X',root=record(b'\x81          ',0x10,5),expected_return=0))
    samples.append(synthetic(b'C:\\A.1234567890',root=record(b'A       123'),
                             expected_return=1,expected_record=record(b'A       123')))
    for missing in (b'C:\\NOPE\\FILE.X',b'C:\\A\\NOPE\\FILE.X',b'C:\\A\\MISSING.X'):
        samples.append(synthetic(missing,expected_return=0))
    for name,expected in ((b'file.x',b'FILE    X  \0'),(b'C:\\DIR\\a.b',b'A       B  \0'),
                          (b'NOEXT',b'NOEXT      \0'),(b'abc.def.ghi',b'ABC     DEF\0'),
                          (b'',b'           \0')):
        samples.append(synthetic(name,function=1,expected_name83=expected))
    for name in (b'.',b'..',b'ABCDEFGH.XYZ',b'ABCDEFGHI.XYZ',b'A.1234567890',b'FOO\\',b'a\x81.x',b'a/b.x'):
        samples.append(synthetic(name,function=1))
    samples.append(synthetic(b'abc',function=1,in_place=True,expected_name83=b'ABC        \0'))
    # Directory paths use the real parent chain and return its final record,
    # without reading the final directory's contents.
    def directory_paths(contents, prefix=(), seen=None):
        seen = set() if seen is None else seen
        for offset in range(0, len(contents), 32):
            item = contents[offset:offset+32]
            if not item[0]: break
            if item[0] in (0xe5, 46) or item[11]&8 or not item[11]&16: continue
            name = item[:8].rstrip()+(b'.'+item[8:11].rstrip() if item[8:11].strip() else b'')
            path = (*prefix, name)
            yield b'C:\\'+b'\\'.join(path), item
            cluster = word(item, 26)
            assert cluster not in seen
            seen.add(cluster)
            data = b''.join(disk.data[o:o+disk.cluster_bytes] for o in disk.chain(cluster))
            yield from directory_paths(data, path, seen)
    for path, item in directory_paths(disk.root):
        depth = path.count(b'\\')
        for name in (path, path.lower(), path+b'\\'):
            samples.append(dict(function=2, name=name, bpb=source[17*512:18*512],
                fat=disk.fat, root=disk.root, disk=source, expected_return=1,
                expected_record=item, expected_allocations=depth-1, expected_frees=depth-1))
    for path, item in walk(disk, disk.root):
        samples.append(dict(function=2, name=path, bpb=source[17*512:18*512],
            fat=disk.fat, root=disk.root, disk=source, expected_return=0))
    for name in malformed:
        samples.append(synthetic(name, function=2))
    for name in (b'C:', b'C:\\', b'C:EXTRA', b'D:\\A', b'C:/A'):
        samples.append(synthetic(name, function=2, expected_return=0))
    for name in (b'C:\\A', b'c:\\a', b'C:EXTRA\\A', b'C:\\A\\'):
        samples.append(synthetic(name, function=2, expected_return=1,
            expected_record=record(b'A          ',0x10,5,0), expected_allocations=0, expected_frees=0))
    for attributes in range(256):
        samples.append(synthetic(b'C:\\A', function=2, root=record(b'A          ',attributes,5,0),
            expected_return=1 if attributes&16 else 0))
    for prefix in (bytes(32),record(b'OTHER      '),record(b'\xe5          ',0x10),record(b'A          ',8)):
        samples.append(synthetic(b'C:\\A', function=2,
            root=prefix+record(b'A          ',0x10,5,0), expected_return=0 if prefix[0]==0 else 1))
    for depth in (1,2,3,10,30,31,32,33):
        directory=record(b'A          ',0x10,5,0)+bytes(32)
        for suffix in (b'A', b'MISSING'):
            loaded=min(depth-1,30)
            success = suffix == b'A'
            samples.append(synthetic(b'C:\\'+b'A\\'*(depth-1)+suffix, function=2,
                directory=directory, expected_return=int(success or depth>31),
                expected_allocations=loaded, expected_frees=loaded))
        samples.append(synthetic(b'C:\\'+b'A\\'*(depth-1)+b'A', function=2,
            directory=directory, free_mutation=True))
    for fail in (1,2):
        samples.append(synthetic(b'C:\\A\\A\\A', function=2,
            directory=record(b'A          ',0x10,5,0)+bytes(32), fail_allocation=fail,
            expected_return=0))
    changed=bytearray(record(b'A          ',0x10,5,0)); struct.pack_into('<H',changed,26,7)
    samples.append(synthetic(b'C:\\A\\A', function=2,
        directory=record(b'A          ',0x10,5,0)+bytes(32), free_mutation=True,
        expected_return=1, expected_record=bytes(changed), expected_allocations=1, expected_frees=1))
    samples.append(synthetic(b'C:\\\x81', function=2,
        root=record(b'\x81          ',0x10,5), expected_return=0))
    samples.append(synthetic(b'C:\\A.1234567890', function=2,
        root=record(b'A       123',0x10), expected_return=1,
        expected_record=record(b'A       123',0x10)))
    return samples


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installed',action='store_true')
    arguments=parser.parse_args()
    started=time.monotonic()
    source=(ROOT.parent/'IceStarOS_x86_32M.vhd').read_bytes()
    disk=Fat16(source); original=disk.read_kernel()
    assert hashlib.sha256(original).hexdigest()==ORIGINAL_SHA256
    variants,sizes=compile_variants(original,arguments.installed)
    machines=[Machine('original',original,b'',0,[s[2] for s in SLOTS])]+[Machine(*v) for v in variants]
    samples=cases(disk,source)
    for index,case in enumerate(samples):
        expected=machines[0].execute(case)
        for machine in machines[1:]:
            actual=machine.execute(case)
            assert actual==expected,(index,case['name'],machine.label,
                [(i,a[:32] if isinstance(a,bytes) else a,b[:32] if isinstance(b,bytes) else b)
                 for i,(a,b) in enumerate(zip(expected,actual)) if a!=b])
    summary=dict(installed=arguments.installed,scenarios=len(samples),executions=len(samples)*3,
        corpus_files=len(list(walk(disk,disk.root))),slots=sizes,seconds=round(time.monotonic()-started,3),
        boundaries='Only allocation/free and hardware sectors; no pathname/directory callbacks',
        limitations='Original unchecked inputs, leaks and free-before-read ordering retained; unbounded alias/cyclic FAT cases excluded',
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest()
                for p in ('kernel/fat_path.c','kernel/fat_path.h','kernel/fat_directory_path.c','tests/kernel_fat_path.py')})
    (BUILD/'fat_path_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2))


if __name__=='__main__': main()
