"""Compare original and recovered cmap dispatch without intercepting helpers.

The ordinary version and installed kernel both execute all nested helpers.
Formats 0/4/6/12/13, original unsupported format 2, signed bounds and
real Fira charmaps are compared with independent table-derived expectations.
"""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import sys
import time
ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(BUILD/'test-deps')]
from tools.kernel_image import Fat16,ORIGINAL_SHA256
from tests.kernel_x_loader import FLAGS,run
from unicorn import Uc,UcError,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_HOOK_MEM_WRITE,UC_HOOK_MEM_INVALID
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,
    UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,
    UC_X86_REG_ESP,UC_X86_REG_EFLAGS,UC_X86_REG_EIP)
BASE,DATA,BUFFER,OUTPUT,TAG,FONT=0x800000,0x600000,0x500000,0x510000,0x520000,0xa00000
STACK,STOP,MASK=0x40f000,0x200000,0xffffffff
SAVED=(UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP)
SENTINELS=(0x11223344,0x55667788,0x19283746,0x31415926)
FLOAT=set()
VOID=set()
F32,F80,CONTROL,START=0x530000,0x530010,0x530020,0x210000
SLOTS=[('font_cmap','kernel_font_glyph_index_82ca7c',0x82ca7c,0x82cede)]


def compile_variants(original, installed):
    obj = BUILD / 'kernel_font_cmap_preview.o'
    run(['gcc', *FLAGS, '-c', 'kernel/font_cmap.c', '-o', str(obj)])
    outputs, sizes = [], {}
    for fixed in (False, True):
        label = 'fixed' if fixed else 'ordinary'
        script, elf = BUILD/f'font_cmap_{label}.ld', BUILD/f'font_cmap_{label}.elf'
        sections = '\n'.join(
            f'.{name} {start:#x} : {{ *(.text.{symbol}) }}\n'
            f'ASSERT({symbol} == {start:#x} && SIZEOF(.{name}) <= {end-start}, "slot overflow")'
            for name, symbol, start, end in SLOTS) if fixed else '. = 0x300000; .text : { *(.text*) }'
        script.write_text('OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_font_glyph_index_82ca7c)\n'
            'PROVIDE(kernel_font_u16_82be14 = 0x82be14);\n'
            'PROVIDE(kernel_font_s16_82be34 = 0x82be34);\n'
            'PROVIDE(kernel_font_u32_82be52 = 0x82be52);\n'
            'PROVIDE(kernel_font_table_82bfe6 = 0x82bfe6);\n'
            'PROVIDE(kernel_font_svg_82c34c = 0x82c34c);\n'
            'PROVIDE(kernel_font_glyph_index_82ca7c = 0x82ca7c);\nSECTIONS {\n' + sections +
            '\n/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n'
            '.unexpected : { *(*) }\nASSERT(SIZEOF(.unexpected)==0, "unexpected section")\n}\n')
        run(['ld', '-m', 'elf_i386', '-T', str(script), '-o', str(elf), str(obj)])
        symbols = {line.split()[-1]: int(line.split()[0], 16)
            for line in run(['nm', str(elf)], capture_output=True, text=True).stdout.splitlines()}
        if fixed:
            candidate = bytearray(original)
            for name, symbol, start, end in SLOTS:
                path = BUILD/f'font_cmap_preview.{name}.bin'
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
            path = BUILD/'font_cmap_ordinary.bin'
            run(['objcopy', '-O', 'binary', str(elf), str(path)])
            outputs.append((label, path.read_bytes(), 0x300000, [symbols[s] for _,s,_,_ in SLOTS]))
    return outputs, sizes


class Machine:
    def __init__(self,label,binary,base,entries,original,font):
        self.label,self.entries=label,entries
        self.uc=Uc(UC_ARCH_X86,UC_MODE_32)
        self.uc.mem_map(0,0x1000000)
        self.uc.mem_map(0xffff0000,0x10000)
        self.uc.mem_write(BASE,original)
        self.uc.mem_write(base,binary)
        self.uc.mem_write(FONT,font)
        self.uc.hook_add(UC_HOOK_CODE,self.finish,begin=STOP+12,end=STOP+12)
        self.uc.hook_add(UC_HOOK_MEM_WRITE,self.written)
        self.uc.hook_add(UC_HOOK_MEM_INVALID,self.invalid)
        self.old_pages=set()

    def invalid(self,uc,access,address,size,value,unused):
        self.fault=(access,address,size)
        return False

    def finish(self,uc,address,size,unused):
        self.done=True
        uc.emu_stop()

    def mark(self,address,size):
        while size:
            self.pages.add(address&~4095)
            step=min(size,4096-(address&4095))
            address,size=(address+step)&MASK,size-step

    def written(self,uc,access,address,size,value,unused):
        if not 0x400000<=address<0x410000:
            self.mark(address,size)

    def put(self,address,data):
        end=min(len(data),0x100000000-address)
        self.uc.mem_write(address,data[:end])
        if end<len(data):self.uc.mem_write(0,data[end:])
        self.mark(address,len(data))

    def execute(self,case):
        uc=self.uc
        for page in self.old_pages:uc.mem_write(page,bytes(4096))
        self.pages,self.done,self.fault=set(),False,None
        # Nonzero output sentinels expose partial/incorrect structure writes.
        self.put(OUTPUT,bytes([0xa5])*256)
        for address,data in case.get('setup',[]):self.put(address,data)
        arguments=[x&MASK for x in case['args']]
        uc.mem_write(CONTROL,struct.pack('<H',case.get('control',0x37f)))
        entry=self.entries[case['function']]
        uc.mem_write(START,b'\xdb\xe3\xd9\x2d'+struct.pack('<I',CONTROL)+b'\xe9'+struct.pack('<I',(entry-START-13)&MASK))
        uc.mem_write(STOP,(b'\xd9\x15'+struct.pack('<I',F32)+b'\xdb\x3d'+struct.pack('<I',F80)) if case['function'] in FLOAT else b'\x90'*12)
        uc.ctl_remove_cache(START,START+16)
        uc.ctl_remove_cache(STOP,STOP+16)
        stack=struct.pack('<'+'I'*(len(arguments)+1),STOP,*arguments)
        uc.mem_write(STACK-0x4000,bytes(0x5000))
        uc.mem_write(STACK,stack)
        uc.reg_write(UC_X86_REG_ESP,STACK)
        uc.reg_write(UC_X86_REG_EFLAGS,2)
        for reg,value in zip(SAVED,SENTINELS):uc.reg_write(reg,value)
        try:
            uc.emu_start(START,0,count=2000000)
        except UcError:
            assert self.fault is not None
            assert self.fault==case.get('expected_fault'),(self.label,self.fault,case)
            self.old_pages=self.pages
            return ('fault',self.fault)
        assert self.done,(self.label,case,hex(uc.reg_read(UC_X86_REG_EIP)))
        assert uc.reg_read(UC_X86_REG_ESP)==STACK+4
        assert tuple(uc.reg_read(reg) for reg in SAVED)==SENTINELS,'callee-saved registers'
        caller=bytes(uc.mem_read(STACK+4,len(arguments)*4))
        assert caller==stack[4:],'caller arguments changed'
        result=uc.reg_read(UC_X86_REG_EAX) if case['function'] not in VOID|FLOAT else None
        if 'expected_return' in case:assert result==case['expected_return'],(self.label,result,case['expected_return'])
        for address,data in case.get('expected_memory',[]):
            actual=bytes(uc.mem_read(address,len(data)))
            assert actual==data,(self.label,hex(address),actual,data)
        floating=bytes(uc.mem_read(F32,4))+bytes(uc.mem_read(F80,10)) if case['function'] in FLOAT else None
        if 'expected_float' in case:assert floating[:4]==struct.pack('<f',case['expected_float']), (floating,case)
        memory=[(page,hashlib.sha256(uc.mem_read(page,4096)).hexdigest()) for page in sorted(self.pages)]
        self.old_pages=self.pages
        return result,caller,memory,floating


def words(*items):return struct.pack('<'+'I'*len(items),*(x&MASK for x in items))

def fixture(payload,codepoint,expected=None,offset=64,data=DATA):
    info=bytearray(64)
    struct.pack_into('<I',info,4,data)
    struct.pack_into('<I',info,48,offset&MASK)
    case=dict(function=0,args=[BUFFER,codepoint],setup=[(BUFFER,bytes(info)),((data+offset)&MASK,payload)])
    if expected is not None:case['expected_return']=expected&MASK
    return case


def format4(segments):
    # Each tuple contains start, end, signed delta and optional glyph array.
    n=len(segments);power=1<<(n.bit_length()-1)
    raw=bytearray(16+8*n)
    struct.pack_into('>HHHHHHH',raw,0,4,len(raw),0,n*2,power*2,power.bit_length()-1,(n-power)*2)
    mapping={}
    for i,(first,last,delta,glyphs) in enumerate(segments):
        struct.pack_into('>H',raw,14+2*i,last)
        struct.pack_into('>H',raw,16+2*n+2*i,first)
        struct.pack_into('>H',raw,16+4*n+2*i,delta&0xffff)
        if glyphs is not None:
            field=16+6*n+2*i
            struct.pack_into('>H',raw,field,len(raw)-field)
            raw.extend(struct.pack('>'+'H'*len(glyphs),*glyphs))
        for codepoint in range(first,last+1):
            # The original omits idDelta when idRangeOffset is nonzero.
            mapping[codepoint]=((codepoint+delta)&0xffff if glyphs is None else glyphs[codepoint-first])
    struct.pack_into('>H',raw,2,len(raw))
    return bytes(raw),mapping


def groups(format_number,records):
    raw=struct.pack('>HHIII',format_number,0,16+len(records)*12,0,len(records))
    raw+=b''.join(struct.pack('>III',*r) for r in records)
    return raw


def independent_map(font,offset):
    format_number=struct.unpack_from('>H',font,offset)[0]
    mapping={}
    if format_number==4:
        n=struct.unpack_from('>H',font,offset+6)[0]//2
        for i in range(n):
            first=struct.unpack_from('>H',font,offset+16+2*n+2*i)[0]
            last=struct.unpack_from('>H',font,offset+14+2*i)[0]
            delta=struct.unpack_from('>h',font,offset+16+4*n+2*i)[0]
            field=offset+16+6*n+2*i
            distance=struct.unpack_from('>H',font,field)[0]
            for codepoint in range(first,last+1):
                mapping[codepoint]=(codepoint+delta)&0xffff if not distance else struct.unpack_from('>H',font,field+distance+2*(codepoint-first))[0]
    elif format_number in (12,13):
        for i in range(struct.unpack_from('>I',font,offset+12)[0]):
            first,last,glyph=struct.unpack_from('>III',font,offset+16+i*12)
            for codepoint in range(first,last+1):mapping[codepoint]=glyph+(codepoint-first if format_number==12 else 0)
    else:raise AssertionError(format_number)
    return mapping


def scenarios(font):
    cases=[]
    for length in (0,1,5,6,7,16,262,65535):
        raw=bytearray(0x10010)
        struct.pack_into('>HHH',raw,0,0,length,0)
        for i in range(256):raw[6+i]=(i*17+9)&255
        for codepoint in (-7,-1,0,1,9,10,255,256,length-6,length-7,65535,0x7fffffff,0x80000000,0xffffffff):
            signed=codepoint if -0x80000000<=codepoint<0x80000000 else codepoint-0x100000000
            expected=(raw[6+signed] if 0<=6+signed<len(raw) else 0) if signed<length-6 else 0
            # Before-table bytes are mapped and initialized to zero.
            case=fixture(bytes(raw),codepoint,expected)
            if signed == -0x80000000:
                # Signed comparison permits the original unmapped byte read.
                from unicorn import UC_MEM_READ_UNMAPPED
                case.pop('expected_return')
                case['expected_fault']=(UC_MEM_READ_UNMAPPED,(DATA+64+6+signed)&MASK,1)
            cases.append(case)
    for first in (0,1,255,65530):
        for count in (0,1,5):
            raw=struct.pack('>HHHHH',6,10+count*2,0,first,count)+b''.join(struct.pack('>H',100+i) for i in range(count))
            for codepoint in (-1,0,first-1,first,first+1,first+count-1,first+count,65535,0x10000,0x80000000):
                expected=100+codepoint-first if first<=codepoint<first+count else 0
                cases.append(fixture(raw,codepoint,expected))
    for format_number in (1,2,3,5,8,10,14,65535):
        for codepoint in (0,65,65535,0x10ffff,0xffffffff):
            cases.append(fixture(struct.pack('>H',format_number),codepoint,0))
    segment_sets=[[(65,70,-3,None),(0xffff,0xffff,1,None)],
        [(0,0,0,None),(32,34,0,[0,102,103]),(100,104,-300,None),(0xffff,0xffff,1,None)],
        [(0,5,17,[8,9,0,11,12,13]),(65,67,-4,[50,0,52]),(100,101,0,None),
         (128,130,32767,None),(256,259,-32768,None),(0xffff,0xffff,1,None)]]
    for segments in segment_sets:
        raw,mapping=format4(segments)
        points=set(range(300))|{0xfffe,0xffff,0x10000,0x10ffff,0x7fffffff,0x80000000,0xffffffff}
        for codepoint in points:cases.append(fixture(raw,codepoint,mapping.get(codepoint,0)))
        for offset in (0,1,0xffffffc0):
            for codepoint in (0,65,66,70,0xffff):cases.append(fixture(raw,codepoint,mapping.get(codepoint,0),offset=offset))
    for format_number in (12,13):
        records=[(0,3,1),(65,70,100),(0x10000,0x10004,200),(0x1f600,0x1f602,300),
                 (0x80000000,0x80000003,0xfffffffe),(0xfffffffe,0xffffffff,0xffff)]
        for n in (0,1,3,len(records)):
            raw=groups(format_number,records[:n])
            for codepoint in (0,1,3,4,64,65,68,70,71,65535,65536,65540,65541,0x1f600,0x1f601,
                0x7fffffff,0x80000000,0x80000001,0xfffffffd,0xfffffffe,0xffffffff):
                match=next((r for r in records[:n] if r[0]<=codepoint<=r[1]),None)
                expected=(match[2]+(codepoint-match[0] if format_number==12 else 0))&MASK if match else 0
                cases.append(fixture(raw,codepoint,expected))
        for count in (0x80000000,0xffffffff):
            raw=bytearray(groups(format_number,[]));struct.pack_into('>I',raw,12,count)
            cases.append(fixture(bytes(raw),65,0))
    # Recover the actual cmap selection policy from the original initializer:
    # later Unicode/platform-3 encoding-1/10 records replace earlier ones.
    tables={}
    for i in range(struct.unpack_from('>H',font,4)[0]):
        tag,checksum,offset,length=struct.unpack_from('>4sIII',font,12+i*16);tables[tag]=offset
    cmap=tables[b'cmap'];records=[];selected=None
    for i in range(struct.unpack_from('>H',font,cmap+2)[0]):
        platform,encoding,relative=struct.unpack_from('>HHI',font,cmap+4+i*8)
        offset=cmap+relative
        if platform==0 or (platform==3 and encoding in (1,10)):
            selected=offset
            if offset not in records:records.append(offset)
    for offset in records:
        mapping=independent_map(font,offset)
        points=(set(range(65536)) if offset==selected else set(range(256))|set(mapping))
        points|={0x10000,0x10ffff,0xffffffff}
        points|={p+1 for p in mapping if p<0x10ffff}
        info=bytearray(64);struct.pack_into('<I',info,4,FONT);struct.pack_into('<I',info,48,offset)
        for codepoint in sorted(points):
            cases.append(dict(function=0,args=[BUFFER,codepoint],setup=[(BUFFER,bytes(info))],
                expected_return=mapping.get(codepoint,0)&MASK))
    return cases,records,selected


def main():
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--installed',action='store_true')
    args=parser.parse_args();started=time.monotonic()
    disk=Fat16((ROOT.parent/'IceStarOS_x86_32M.vhd').read_bytes());original=disk.read_kernel()
    assert hashlib.sha256(original).hexdigest()==ORIGINAL_SHA256
    font=disk.read_file(b'ICESTAR    ',b'FONT       ',b'FIRA    TTF')
    variants,sizes=compile_variants(original,args.installed)
    specs=[('original',original,BASE,[SLOTS[0][2]]),*variants]
    machines=[Machine(*s,original,font) for s in specs]
    cases,records,selected=scenarios(font)
    for index,case in enumerate(cases):
        try:
            expected=machines[0].execute(case)
            for machine in machines[1:]:
                actual=machine.execute(case)
                assert actual==expected,(machine.label,expected,actual)
        except Exception:
            print('Failing case',index,{k:v for k,v in case.items() if k!='setup'},flush=True);raise
        if index and index%10000==0:print(f'Compared {index}/{len(cases)} cmap scenarios',flush=True)
    summary=dict(installed=args.installed,scenarios=len(cases),executions=len(cases)*3,slots=sizes,
        seconds=round(time.monotonic()-started,3),boundary_scope='No intercepted helpers',
        original_fira_maps=[hex(x) for x in records],original_fira_selected_map=hex(selected),
        selected_map_all_bmp_codepoints_checked=True,formats=[0,4,6,12,13],
        checks=['independent table-derived glyph expectations','all touched data pages','returns',
                'callee-saved registers','caller arguments','signed bounds and original range-offset semantics'],
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in
            ('kernel/font_cmap.c','kernel/font_cmap.h','tests/kernel_font_cmap.py')})
    (BUILD/'font_cmap_summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))


if __name__=='__main__':main()
