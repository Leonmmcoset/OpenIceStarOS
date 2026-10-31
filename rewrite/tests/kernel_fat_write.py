"""Execute recovered FAT path splitter/file writers against the original x86.

The ordinary and fixed-address variants run real original filesystem/string
helpers. Only allocation/free and disk transfers are intercepted. Installed runs use the complete
production kernel for the fixed-address variant.
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
from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_ERR_EXCEPTION
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
    UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP,
    UC_X86_REG_ESP, UC_X86_REG_EIP)

SLOTS = [
    ("fat_split_parent", "kernel_fat_split_parent_828018", 0x828018, 0x8280e4),
    ("fat_write_root", "kernel_fat_write_root_82867e", 0x82867e, 0x828b35),
    ("fat_write_path", "kernel_fat_write_path_8280e4", 0x8280e4, 0x82867e),
]
HELPERS = dict(kernel_string_length=0x82a45d, kernel_copy_string=0x82a32b,
    kernel_string_upper_82a531=0x82a531, kernel_x_find_relative_825c8e=0x825c8e,
    kernel_fat_root_empty_82798c=0x82798c, kernel_fat_name83_827bba=0x827bba,
    kernel_fat_release_root_chain_827d4a=0x827d4a,
    kernel_fat_find_free_cluster_827ace=0x827ace,
    kernel_fat_find_free_after_827b44=0x827b44,
    kernel_fat_write_sectors_827c72=0x827c72,
    kernel_fat_flush_metadata_827cc4=0x827cc4,
    kernel_fat_find_directory_path_825f48=0x825f48,
    kernel_fat_read_alloc_827454=0x827454,
    kernel_fat_directory_find_825d34=0x825d34,
    kernel_fat_directory_empty_827a40=0x827a40,
    kernel_fat_release_record_chain_827e6c=0x827e6c,
    kernel_free_allocation=0x815978)
BASE, BPB, NAME, SOURCE, FAT1, FAT2 = 0x800000, 0x520000, 0x600000, 0x610000, 0x500000, 0x540000
PARENT, FILE, STACK, STOP = 0x601000, 0x602000, 0x40f000, 0x200000
ARENA = 0x900000
MASK = 0xffffffff
SOURCE_BYTES = bytes((i*37 + (i>>8)*11) & 255 for i in range(0x60000))


def compile_variants(original, installed):
    obj = BUILD / "kernel_fat_write_preview.o"
    run(["gcc", *FLAGS, "-c", "kernel/fat_write.c", "-o", str(obj)])
    outputs, sizes = [], {}
    for fixed in (False, True):
        label = "fixed" if fixed else "ordinary"
        script, elf = BUILD / f"fat_write_{label}.ld", BUILD / f"fat_write_{label}.elf"
        sections = "\n".join(
            f".{name} {start:#x} : {{ *(.text.{symbol}) }}\n"
            f"ASSERT({symbol} == {start:#x} && SIZEOF(.{name}) <= {end-start}, \"slot overflow\")"
            for name, symbol, start, end in SLOTS) if fixed else ". = 0x300000; .text : { *(.text*) }"
        script.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_fat_split_parent_828018)\n" +
            "\n".join(f"PROVIDE({symbol} = {address:#x});" for symbol, address in HELPERS.items()) +
            "\nSECTIONS {\n" + sections + "\n/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n" +
            '.unexpected : { *(*) }\nASSERT(SIZEOF(.unexpected)==0, "unexpected section")\n}\n')
        run(["ld", "-m", "elf_i386", "-T", str(script), "-o", str(elf), str(obj)])
        symbols = {line.split()[-1]: int(line.split()[0], 16)
                   for line in run(["nm", str(elf)], capture_output=True, text=True).stdout.splitlines()}
        if fixed:
            candidate = bytearray(original)
            for name, symbol, start, end in SLOTS:
                path = BUILD / f"fat_write_preview.{name}.bin"
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
            path = BUILD / "fat_write_candidate.x"
            path.write_bytes(candidate)
            outputs.append(("fixed", bytes(candidate), BASE,
                            [symbols[symbol] for _, symbol, _, _ in SLOTS]))
        else:
            path = BUILD / "fat_write_ordinary.bin"
            run(["objcopy", "-O", "binary", str(elf), str(path)])
            outputs.append(("ordinary", path.read_bytes(), 0x300000,
                            [symbols[symbol] for _, symbol, _, _ in SLOTS]))
    return outputs, sizes


class Machine:
    def __init__(self, label, binary, base, entries, original):
        self.label, self.entries = label, entries
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        self.uc.mem_map(0, 0x1000000)
        self.uc.mem_write(BASE, original)
        self.uc.mem_write(base, binary)
        self.uc.hook_add(UC_HOOK_CODE, self.finish, begin=STOP, end=STOP)
        self.uc.hook_add(UC_HOOK_CODE, self.write_sectors, begin=0x827c72, end=0x827c72)
        for address in (0x815464,0x815978,0x8254ca):
            self.uc.hook_add(UC_HOOK_CODE,self.boundary,begin=address,end=address)
        self.uc.hook_add(UC_HOOK_CODE,self.release_entry,begin=0x827e6c,end=0x827e6c)

    def finish(self, uc, address, size, unused):
        self.done = True
        uc.emu_stop()

    def write_sectors(self, uc, address, size, unused):
        sp = uc.reg_read(UC_X86_REG_ESP)
        lba, source, count = struct.unpack('<3I',uc.mem_read(sp+4,12))
        bps = word(uc.mem_read(BPB+11,2),0)
        payload = bytes(uc.mem_read(source,count*bps))
        self.writes.append((lba,source,count,hashlib.sha256(payload).hexdigest()))
        self.trace.append(('write',self.writes[-1]))
        if len(self.writes) == 1 and self.case.get('mutate'):
            uc.mem_write(BPB+11,struct.pack('<H',256))
            uc.mem_write(BPB+13,b'\x02')
            uc.mem_write(0x1a83a,struct.pack('<II',FAT1+0x1000,FAT2+0x1000))
        uc.reg_write(UC_X86_REG_EAX,0xdeadbeef)
        uc.reg_write(UC_X86_REG_ECX,0x11112222)
        uc.reg_write(UC_X86_REG_EDX,0x33334444)
        uc.reg_write(UC_X86_REG_ESP,sp+4)
        uc.reg_write(UC_X86_REG_EIP,struct.unpack('<I',uc.mem_read(sp,4))[0])

    def boundary(self,uc,address,size,unused):
        sp = uc.reg_read(UC_X86_REG_ESP)
        argc = 3 if address == 0x8254ca else 1
        args = struct.unpack(f'<{argc}I',uc.mem_read(sp+4,argc*4))
        result = 0x7351
        if address == 0x815464:
            self.allocations += 1
            assert self.allocations <= 16
            result = ARENA+(self.allocations-1)*0x10000
            if self.allocations == self.case.get('fail_allocation'): result = 0
            self.trace.append(('allocate',args,result))
        elif address == 0x815978:
            self.trace.append(('free',args))
            if self.case.get('free_mutation'):
                uc.mem_write(args[0]+26,struct.pack('<H',202))
        else:
            lba,count,destination = args
            bps = word(uc.mem_read(BPB+11,2),0)
            assert count <= 32 and ARENA <= destination <= ARENA+0x200000-count*bps,args
            payload = b''.join(self.sectors.get((lba+i)&MASK,bytes(bps)) for i in range(count))
            assert len(payload) == count*bps
            uc.mem_write(destination,payload)
            self.trace.append(('read',args,hashlib.sha256(payload).hexdigest()))
        uc.reg_write(UC_X86_REG_EAX,result)
        uc.reg_write(UC_X86_REG_ECX,0x11112222)
        uc.reg_write(UC_X86_REG_EDX,0x33334444)
        uc.reg_write(UC_X86_REG_ESP,sp+4)
        uc.reg_write(UC_X86_REG_EIP,struct.unpack('<I',uc.mem_read(sp,4))[0])

    def release_entry(self,uc,address,size,unused):
        # Let the real release routine execute. Change geometry at this
        # boundary to observe subsequent timestamp/size writes before DIV.
        if self.case.get('release_zero') == 'bps': uc.mem_write(BPB+11,b'\0\0')
        if self.case.get('release_zero') == 'spc': uc.mem_write(BPB+13,b'\0')

    def path_fixture(self,case,root,directory,bps,spc,entries):
        depth = case.get('depth',1)
        folders = (b'DIR        ',b'SUB        ',b'THIRD      ',b'LAST       ')
        assert 1 <= depth <= len(folders)
        parent = bytearray(32)
        parent[:11],parent[11] = folders[0],0x10
        struct.pack_into('<H',parent,20,case.get('parent_high',0))
        struct.pack_into('<H',parent,26,200)
        self.uc.mem_write(root+32,bytes(parent))
        # A non-directory parent must fail lookup before reading any clusters.
        if case.get('parent_file'): self.uc.mem_write(root+32+11,b'\x20')
        start = ((entries*32+bps-1)//bps if bps else 0)+1+case.get('fats',2)*case.get('fat_sectors',4)+case.get('partition',17)
        self.sectors = {}
        for level in range(depth):
            block = directory if level == depth-1 else bytearray(bps*spc)
            if level < depth-1:
                parent[:11] = folders[level+1]
                struct.pack_into('<H',parent,26,201+level)
                block[:32] = parent
            block = bytes(block).ljust(bps*spc,b'\0')
            if bps:
                for i in range(spc):
                    self.sectors[(start+(198+level)*spc+i)&MASK] = block[i*bps:(i+1)*bps]

    def execute(self, function, case):
        uc = self.uc
        self.case,self.done,self.writes,self.trace,self.allocations = case,False,[],[],0
        uc.mem_write(0,bytes(0x20000))
        uc.mem_write(0x500000,bytes(0x200000))
        uc.mem_write(STACK-0x4000,bytes(0x5000))
        if function == 2: uc.mem_write(ARENA,b'\xa5'*0x200000)
        uc.mem_write(NAME,case.get('name',b'new.tst')+b'\0')
        uc.mem_write(PARENT,b'\xa5'*512)
        uc.mem_write(FILE,b'\x5a'*512)
        if function == 0:
            parent = NAME+case['parent_alias'] if 'parent_alias' in case else PARENT
            filename = NAME+case['file_alias'] if 'file_alias' in case else FILE
            args = (NAME,parent,filename)
        else:
            bps,spc,fats,sectors,entries = [case.get(k,v) for k,v in
                (('bps',512),('spc',4),('fats',2),('fat_sectors',4),('entries',8))]
            bpb = bytearray(512)
            struct.pack_into('<H',bpb,11,bps)
            bpb[13] = spc
            struct.pack_into('<H',bpb,14,1)
            bpb[16] = fats
            struct.pack_into('<H',bpb,17,entries)
            struct.pack_into('<H',bpb,22,sectors)
            uc.mem_write(BPB,bytes(bpb))
            uc.mem_write(0x7dc6,struct.pack('<I',case.get('partition',17)))
            uc.mem_write(0x1a83a,struct.pack('<II',FAT1,FAT2))
            table = bytearray(b'\xff'*0x20000)
            for cluster in case.get('free',range(2,128)):
                struct.pack_into('<H',table,cluster*2,0)
            for cluster,next_cluster in case.get('old_chain',{6:9,9:0xffff}).items():
                struct.pack_into('<H',table,cluster*2,next_cluster)
            uc.mem_write(FAT1,bytes(table))
            uc.mem_write(FAT2,bytes(table))
            records = max(entries,bps*spc//32) if function == 2 and not case.get('root_forward') else entries
            directory = bytearray(b'FILLER  TST'+bytes([0x20])+bytes(20))*records
            mode = case.get('mode','new')
            index = case.get('index',3)
            root = BPB+(1+fats*sectors)*bps
            if mode == 'new':
                directory[index*32:(index+1)*32] = bytes(32)
            elif mode in ('existing','deleted'):
                record = bytearray(32)
                record[:11] = b'NEW     TST'
                record[11] = case.get('attributes',0x20)
                struct.pack_into('<H',record,26,6)
                struct.pack_into('<I',record,28,case.get('old_size',4096))
                if mode == 'deleted': record[0] = 0xe5
                directory[index*32:(index+1)*32] = record
            if function == 2 and not case.get('root_forward'):
                if bps:
                    uc.mem_write(root,(b'FILLER  TST'+bytes([0x20])+bytes(20))*entries)
                self.path_fixture(case,root,directory,bps,spc,entries)
            elif bps:
                uc.mem_write(root,bytes(directory))
            uc.mem_write(SOURCE,SOURCE_BYTES)
            args = (NAME,SOURCE,case.get('bytes',513))
        uc.mem_write(STACK,struct.pack('<6I',STOP,*args,0x13579bdf,0x2468ace0))
        saved = [(UC_X86_REG_EBX,0x12345678),(UC_X86_REG_ESI,0x87654321),
                 (UC_X86_REG_EDI,0xabcdef12),(UC_X86_REG_EBP,0x11223344)]
        for reg,value in [*saved,(UC_X86_REG_EAX,0x99887766),(UC_X86_REG_ECX,0x55443322),
                          (UC_X86_REG_EDX,0xcafebabe),(UC_X86_REG_ESP,STACK)]:
            uc.reg_write(reg,value)
        fault = None
        try:
            uc.emu_start(self.entries[function],STOP+1,count=500000)
        except UcError as error:
            assert case.get('fault') and error.errno == UC_ERR_EXCEPTION,(self.label,case,error)
            fault = error.errno
        assert bool(fault) == bool(case.get('fault')), (self.label,case,'division fault mismatch')
        if not fault:
            assert self.done,(self.label,case,hex(uc.reg_read(UC_X86_REG_EIP)))
            assert uc.reg_read(UC_X86_REG_ESP) == STACK+4
            assert all(uc.reg_read(reg)==value for reg,value in saved)
            assert bytes(uc.mem_read(STACK+4,20)) == struct.pack('<5I',*args,0x13579bdf,0x2468ace0), (self.label, case, 'caller arguments changed')
        eax = None if fault else uc.reg_read(UC_X86_REG_EAX)
        if function == 0 and not ('parent_alias' in case or 'file_alias' in case):
            name = case['name']
            separator = name.rfind(b'\\')
            assert eax == int(separator>0)
            if separator>0:
                assert bytes(uc.mem_read(PARENT,separator+1)) == name[:separator]+b'\0'
                assert bytes(uc.mem_read(FILE,len(name)-separator)) == name[separator+1:]+b'\0'
            else:
                assert bytes(uc.mem_read(PARENT,512)) == b'\xa5'*512
                assert bytes(uc.mem_read(FILE,512)) == b'\x5a'*512
        if function in (1,2) and not fault and not case.get('mutate'):
            expected_sectors = ((case.get('bytes',513)+case.get('bps',512)-1)&MASK)//case.get('bps',512)
            rejected = (case.get('mode') == 'full' or
                (case.get('mode') == 'existing' and
                 ((case.get('index',3) == 0 and (function == 1 or case.get('root_forward'))) or
                  case.get('attributes',0x20) & 0x18)))
            if function == 2:
                rejected = rejected or case.get('rejected') or bool(case.get('fail_allocation'))
            assert eax == (0 if rejected else expected_sectors), (self.label, case, eax, expected_sectors)
            if rejected:
                assert not self.writes
            else:
                assert self.writes[-1][1] == FAT1, 'metadata was not flushed last'
                data = [x for x in self.writes if SOURCE <= x[1] < SOURCE+len(SOURCE_BYTES)]
                clusters = (expected_sectors+case.get('spc',4)-1)//case.get('spc',4)
                assert len(data)==clusters,(self.label,case,len(data),clusters)
        result = (fault,eax,self.writes,hashlib.sha256(uc.mem_read(0,0x20000)).hexdigest(),
                  hashlib.sha256(uc.mem_read(0x500000,0x200000)).hexdigest())
        if function == 2:
            result += (self.trace,hashlib.sha256(uc.mem_read(ARENA,0x200000)).hexdigest())
        return result


def make_cases():
    cases = []
    names = [b'',b'A',b'\\',b'\\A',b'X\\',b'C:',b'C:\\',b'C:\\A.TST',
             b'C:\\ICESTAR\\NEW.TST',b'a\\b\\c',b'a/b',b'\\\\A',b'a\\\\b',b'a\\b\\',
             b'c:\\\x80\xff.tst']
    names += [b'a'*n+b'\\'+b'b'*m for n in (1,2,31,127) for m in (0,1,15,63)]
    for name in names:
        cases.append((0,dict(name=name)))
        for parent_alias,file_alias in ((0,64),(1,128),(64,0),(0,0),(4,8)):
            cases.append((0,dict(name=name,parent_alias=parent_alias,file_alias=file_alias)))
    for mode in ('new','existing','deleted','full'):
        for bps,spc in ((128,1),(128,4),(512,1),(512,4),(512,16),(1024,2)):
            for size in (0,1,bps-1,bps,bps+1,bps*spc-1,bps*spc,bps*spc+1,bps*spc*2+1,
                         0xfffffff0,0xffffffff):
                cases.append((1,dict(mode=mode,bps=bps,spc=spc,bytes=size)))
    for fats in (0,1,2,3):
        for mode in ('new','existing','deleted'):
            cases.append((1,dict(mode=mode,fats=fats,bytes=4097)))
    for free in ([],[2],[2,3],[2,5],[2,127]):
        cases.append((1,dict(free=free,bytes=4097)))
    for index in (0,1,7):
        cases.append((1,dict(mode='existing',index=index)))
    for attributes in (0x01,0x10,0x20,0xff):
        cases.append((1,dict(mode='existing',attributes=attributes)))
    for name in (b'New.Tst',b'C:\\NEW.TST',b'abcdefghi.xyz',b'one.abcdef',b'\x80\xff.tst',b'NEW'):
        cases.append((1,dict(name=name)))
    for bps,spc in ((0,4),(512,0),(0,0)):
        cases.append((1,dict(bps=bps,spc=spc,fault=True)))
    cases.append((1,dict(mutate=True,bytes=4097)))
    cases.append((1,dict(partition=0xfffffff0,bytes=513)))
    path = b'c:\\dir\\new.tst'
    for mode in ('new','existing','deleted','full'):
        for bps,spc in ((128,1),(128,4),(512,1),(512,4),(512,16),(1024,2)):
            for size in (0,1,bps-1,bps,bps+1,bps*spc-1,bps*spc,bps*spc+1,bps*spc*2+1,
                         0xfffffff0,0xffffffff):
                cases.append((2,dict(name=path,mode=mode,bps=bps,spc=spc,bytes=size)))
    for depth,name in enumerate((b'C:\\DIR\\NEW.TST',b'C:\\DIR\\SUB\\NEW.TST',
            b'C:\\DIR\\SUB\\THIRD\\NEW.TST',b'C:\\DIR\\SUB\\THIRD\\LAST\\NEW.TST'),1):
        for mode in ('new','existing','deleted','full'):
            cases.append((2,dict(name=name,depth=depth,mode=mode,bytes=4097)))
        for fail in range(1,depth+1):
            cases.append((2,dict(name=name,depth=depth,fail_allocation=fail)))
    for index in (0,1,7):
        for mode in ('new','existing','deleted'):
            cases.append((2,dict(name=path,mode=mode,index=index)))
    for fats in (0,1,2,3):
        for mode in ('new','existing','deleted'):
            cases.append((2,dict(name=path,mode=mode,fats=fats,bytes=4097)))
    for free in ([],[2],[2,3],[2,5],[2,127]):
        cases.append((2,dict(name=path,free=free,bytes=4097)))
    for attributes in (0x01,0x10,0x20,0xff):
        cases.append((2,dict(name=path,mode='existing',attributes=attributes)))
    for name in (b'',b'NEW.TST',b'\\NEW.TST',b'D:\\DIR\\NEW.TST',b'C:\\MISSING\\NEW.TST',
                 b'C:\\DIR\\MISSING\\NEW.TST',b'C:/DIR/NEW.TST',b'C:\\abcdefghijklmnop.tst'):
        cases.append((2,dict(name=name,rejected=True)))
    for name in (b'c:\\dir\\New.Tst',b'C:\\DIR\\abcdefghi.xyz',b'C:\\DIR\\one.abcdef',
                 b'C:\\DIR\\\x80\xff.tst',b'C:\\DIR\\NEW',b'C:\\DIR\\'):
        cases.append((2,dict(name=name)))
    for mode in ('new','existing','deleted','full'):
        for size in (0,1,8193):
            cases.append((2,dict(name=b'c:\\new.tst',root_forward=True,mode=mode,bytes=size)))
    for bps,spc in ((0,4),(512,0),(0,0)):
        cases.append((2,dict(name=path,bps=bps,spc=spc,fault=True)))
    for high in (1,0xffff): cases.append((2,dict(name=path,parent_high=high)))
    cases.append((2,dict(name=path,parent_file=True,rejected=True)))
    cases.append((2,dict(name=path,mutate=True,bytes=4097)))
    cases.append((2,dict(name=path,partition=0xfffffff0,bytes=513)))
    cases.append((2,dict(name=b'C:\\DIR\\SUB\\NEW.TST',depth=2,free_mutation=True)))
    for name in (b'NOSPLIT.TST',b'C:\\MISSING\\NEW.TST'):
        cases.append((2,dict(name=name,bps=0,fault=True)))
    for mode in ('existing','deleted'):
        for divisor in ('bps','spc'):
            cases.append((2,dict(name=path,mode=mode,release_zero=divisor,fault=True)))
    return cases


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installed',action='store_true')
    args = parser.parse_args()
    started = time.monotonic()
    original = Fat16((ROOT.parent/'IceStarOS_x86_32M.vhd').read_bytes()).read_kernel()
    assert hashlib.sha256(original).hexdigest() == ORIGINAL_SHA256
    variants,sizes = compile_variants(original,args.installed)
    specs = [('original',original,BASE,[s[2] for s in SLOTS]),*variants]
    machines = [Machine(*s,original) for s in specs]
    cases = make_cases()
    for number,(function,case) in enumerate(cases):
        current = [Machine(*s,original) for s in specs] if case.get('fault') else machines
        expected = current[0].execute(function,case)
        for machine in current[1:]:
            actual = machine.execute(function,case)
            assert actual == expected,(number,function,case,machine.label,
                [i for i,(a,b) in enumerate(zip(expected,actual)) if a!=b],
                expected[:3],actual[:3])
    summary = dict(installed=args.installed,scenarios=len(cases),executions=len(cases)*3,slots=sizes,
        seconds=round(time.monotonic()-started,3),
        original_kernel_sha256=hashlib.sha256(original).hexdigest(),
        candidate_kernel_sha256=hashlib.sha256(variants[1][1]).hexdigest(),
        scope='Original filesystem/string helpers execute; only allocation/free and sector transfers are intercepted.',
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in
                ('kernel/fat_write.c','kernel/fat_write.h','tests/kernel_fat_write.py')})
    (BUILD/'fat_write_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2))


if __name__ == '__main__':
    main()
