"""Execute 24 CFF/SFNT parsing functions against original x86 without stubs.

The ordinary version and installed kernel both execute all nested helpers.
The 12-byte structure return ABI and by-value buffer parameter mutation are
compared in addition to data, return values, registers and caller arguments.
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
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,
    UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,
    UC_X86_REG_ESP,UC_X86_REG_EFLAGS,UC_X86_REG_EIP)
BASE,DATA,BUFFER,OUTPUT,TAG,FONT=0x800000,0x600000,0x500000,0x510000,0x520000,0xa00000
STACK,STOP,MASK=0x40f000,0x200000,0xffffffff
SAVED=(UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP)
SENTINELS=(0x11223344,0x55667788,0x19283746,0x31415926)
AGGREGATE={5,6,7,10,13,22}
VOID={2,3,9,11}
SLOTS = [
    ('font_get8', 'kernel_font_get8_82b838', 0x82b838, 0x82b86e),
    ('font_peek8', 'kernel_font_peek8_82b86e', 0x82b86e, 0x82b8a2),
    ('font_seek', 'kernel_font_seek_82b8a2', 0x82b8a2, 0x82b8de),
    ('font_skip', 'kernel_font_skip_82b8de', 0x82b8de, 0x82b8fe),
    ('font_getn', 'kernel_font_getn_82b8fe', 0x82b8fe, 0x82b946),
    ('font_buffer', 'kernel_font_buffer_82b946', 0x82b946, 0x82b962),
    ('font_range', 'kernel_font_range_82b962', 0x82b962, 0x82b9ca),
    ('font_cff_index', 'kernel_font_cff_index_82b9ca', 0x82b9ca, 0x82ba70),
    ('font_cff_int', 'kernel_font_cff_int_82ba70', 0x82ba70, 0x82bb70),
    ('font_cff_skip', 'kernel_font_cff_skip_82bb70', 0x82bb70, 0x82bbe4),
    ('font_cff_dict', 'kernel_font_cff_dict_82bbe4', 0x82bbe4, 0x82bcc8),
    ('font_cff_ints', 'kernel_font_cff_ints_82bcc8', 0x82bcc8, 0x82bd28),
    ('font_cff_count', 'kernel_font_cff_count_82bd28', 0x82bd28, 0x82bd56),
    ('font_cff_item', 'kernel_font_cff_item_82bd56', 0x82bd56, 0x82be14),
    ('font_u16', 'kernel_font_u16_82be14', 0x82be14, 0x82be34),
    ('font_s16', 'kernel_font_s16_82be34', 0x82be34, 0x82be52),
    ('font_u32', 'kernel_font_u32_82be52', 0x82be52, 0x82be86),
    ('font_s32', 'kernel_font_s32_82be86', 0x82be86, 0x82beba),
    ('font_signature', 'kernel_font_signature_82beba', 0x82beba, 0x82bfe6),
    ('font_table', 'kernel_font_table_82bfe6', 0x82bfe6, 0x82c0b2),
    ('font_offset', 'kernel_font_offset_82c0b2', 0x82c0b2, 0x82c196),
    ('font_count', 'kernel_font_count_82c196', 0x82c196, 0x82c232),
    ('font_cff_subrs', 'kernel_font_cff_subrs_82c232', 0x82c232, 0x82c34c),
    ('font_svg', 'kernel_font_svg_82c34c', 0x82c34c, 0x82c3bc),
]


def compile_variants(original, installed):
    obj = BUILD / 'kernel_font_parse_preview.o'
    run(['gcc', *FLAGS, '-c', 'kernel/font_parse.c', '-o', str(obj)])
    outputs, sizes = [], {}
    for fixed in (False, True):
        label = 'fixed' if fixed else 'ordinary'
        script, elf = BUILD/f'font_parse_{label}.ld', BUILD/f'font_parse_{label}.elf'
        sections = '\n'.join(
            f'.{name} {start:#x} : {{ *(.text.{symbol}) }}\n'
            f'ASSERT({symbol} == {start:#x} && SIZEOF(.{name}) <= {end-start}, "slot overflow")'
            for name, symbol, start, end in SLOTS) if fixed else '. = 0x300000; .text : { *(.text*) }'
        script.write_text('OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_font_get8_82b838)\n'
            'SECTIONS {\n' + sections +
            '\n/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n'
            '.unexpected : { *(*) }\nASSERT(SIZEOF(.unexpected)==0, "unexpected section")\n}\n')
        run(['ld', '-m', 'elf_i386', '-T', str(script), '-o', str(elf), str(obj)])
        symbols = {line.split()[-1]: int(line.split()[0], 16)
            for line in run(['nm', str(elf)], capture_output=True, text=True).stdout.splitlines()}
        if fixed:
            candidate = bytearray(original)
            for name, symbol, start, end in SLOTS:
                path = BUILD/f'font_parse_preview.{name}.bin'
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
            path = BUILD/'font_parse_ordinary.bin'
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
        self.uc.hook_add(UC_HOOK_CODE,self.finish,begin=STOP,end=STOP)
        self.uc.hook_add(UC_HOOK_MEM_WRITE,self.written)
        self.old_pages=set()

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
        self.pages,self.done=set(),False
        # Nonzero output sentinels expose partial/incorrect structure writes.
        self.put(OUTPUT,bytes([0xa5])*256)
        for address,data in case.get('setup',[]):self.put(address,data)
        arguments=[x&MASK for x in case['args']]
        stack=struct.pack('<'+'I'*(len(arguments)+1),STOP,*arguments)
        uc.mem_write(STACK-0x4000,bytes(0x5000))
        uc.mem_write(STACK,stack)
        uc.reg_write(UC_X86_REG_ESP,STACK)
        uc.reg_write(UC_X86_REG_EFLAGS,2)
        for reg,value in zip(SAVED,SENTINELS):uc.reg_write(reg,value)
        uc.emu_start(self.entries[case['function']],0,count=2000000)
        assert self.done,(self.label,case,hex(uc.reg_read(UC_X86_REG_EIP)))
        assert uc.reg_read(UC_X86_REG_ESP)==STACK+4+(4 if case['function'] in AGGREGATE else 0)
        assert tuple(uc.reg_read(reg) for reg in SAVED)==SENTINELS,'callee-saved registers'
        caller=bytes(uc.mem_read(STACK+4,len(arguments)*4))
        if case['function'] not in (13,22):assert caller==stack[4:],'caller arguments changed'
        result=uc.reg_read(UC_X86_REG_EAX) if case['function'] not in VOID else None
        if 'expected_return' in case:assert result==case['expected_return'],(self.label,result,case['expected_return'])
        for address,data in case.get('expected_memory',[]):
            actual=bytes(uc.mem_read(address,len(data)))
            assert actual==data,(self.label,hex(address),actual,data)
        # Buffers returned from local subranges contain original data addresses,
        # so all final data pages can be compared directly across variants.
        memory=[(page,hashlib.sha256(uc.mem_read(page,4096)).hexdigest()) for page in sorted(self.pages)]
        self.old_pages=self.pages
        return result,caller,memory


def words(*items):return struct.pack('<'+'I'*len(items),*(x&MASK for x in items))

def stream(function,payload=b'',cursor=0,size=None,extra=(),output=OUTPUT,data=DATA,buffer=BUFFER):
    size=len(payload) if size is None else size
    setup=[(data,payload),(buffer,words(data,cursor,size))]
    args=([output,buffer,*extra] if function in AGGREGATE else [buffer,*extra])
    return dict(function=function,args=args,setup=setup)


def encode(value):
    if -107<=value<=107:return bytes([value+139])
    if 108<=value<=1131:
        value-=108;return bytes([247+(value>>8),value&255])
    if -1131<=value<=-108:
        value=-value-108;return bytes([251+(value>>8),value&255])
    return b'\x1d'+words(value)[::-1]


def index_data(items,width=1):
    offsets=[1]
    for item in items:offsets.append(offsets[-1]+len(item))
    if not items:return b'\0\0'
    return struct.pack('>H',len(items))+bytes([width])+b''.join(o.to_bytes(width,'big') for o in offsets)+b''.join(items)


def scenarios(font):
    cases=[]
    pattern=bytes((i*31+77)&255 for i in range(128))
    for function in range(5):
        for cursor in (-3,-1,0,1,15,16,17,0x7fffffff,0x80000000,0xffffffff):
            for size in (-1,0,1,16,0x80000000):
                if function in (0,1) and (cursor if cursor<0x80000000 else cursor-0x100000000)<(size if size<0x80000000 else size-0x100000000) and abs(cursor)>0x1000:
                    continue
                for extra in ([(0,)] if function<2 else [(x,) for x in (-1,0,1,16,17,0x7fffffff,0x80000000)] if function in (2,3) else [(x,) for x in (-1,0,1,2,4,8)]):
                    if function<2:extra=()
                    if function==4 and (cursor in (0x7fffffff,0x80000000)) and size!=0x80000000:continue
                    case=stream(function,pattern,cursor,size,extra)
                    # Negative cursors intentionally read before data; add a guard.
                    case['setup'].insert(0,(DATA-16,b'0123456789abcdef'))
                    if function in (0,1):
                        signed_cursor=(cursor if cursor<0x80000000 else cursor-0x100000000)
                        signed_size=(size if size<0x80000000 else size-0x100000000)
                        if signed_cursor>=signed_size:case['expected_return']=0
                    cases.append(case)
    for data in (0,DATA,0xfffffff0):
        for size in (0,1,0x7fffffff,0x80000000,0xffffffff):
            cases.append(dict(function=5,args=[OUTPUT,data,size],expected_return=OUTPUT,
                expected_memory=[(OUTPUT,words(data,0,size))]))
    for size in (-1,0,1,16,0x7fffffff,0x80000000):
        for start in (-1,0,1,15,16,0x7fffffff,0x80000000):
            for length in (-1,0,1,16,0x7fffffff,0x80000000):
                for output in (OUTPUT,BUFFER,BUFFER+4,BUFFER-4):
                    cases.append(stream(6,pattern,3,size,(start,length),output))
    for first in range(256):
        for tail in (b'',b'\x00',b'\xff',b'\x12\x34\x56\x78'):
            cases.append(stream(8,bytes([first])+tail))
    for value in (-2147483648,-32768,-1131,-108,-107,0,107,108,1131,32767,2147483647):
        cases.append(dict(stream(8,encode(value)),expected_return=value&MASK))
    for payload in (b'',b'\x1e',b'\x1e\x0f',b'\x1e\xf0',b'\x1e\x12\x34\x5f',b'\x1e\x12\x34',b'\x1c\xff\xff',b'\x1d\x12\x34',b'\x8b'):
        cases.append(stream(9,payload))
    # Valid/truncated/malformed CFF INDEX data, all offset widths, unchecked
    # negative/out-of-range item indexes and by-value cursor mutation.
    for items in ([],[b''],[b'abc'],[b'A',b'BC',b'DEF'],[pattern[:32],b'Z']):
        for width in (1,2,3,4):
            payload=index_data(items,width)
            for cut in sorted(set((0,1,2,3,len(payload)//2,len(payload)-1,len(payload)))):
                if cut<0:continue
                data=payload[:cut]
                cases.append(stream(7,data))
                cases.append(stream(12,data))
                for index in (-1,0,1,len(items),0x7fffffff):
                    cases.append(dict(function=13,args=[OUTPUT,DATA,19,len(data),index],setup=[(DATA,data)]))
            if items:
                for i,item in enumerate(items):
                    start=3+(len(items)+1)*width+sum(map(len,items[:i]))
                    cases.append(dict(function=13,args=[OUTPUT,DATA,99,len(payload),i],setup=[(DATA,payload)],
                        expected_return=OUTPUT,expected_memory=[(OUTPUT,words(DATA+start,0,len(item)))]))
    for width in (0,5,255):
        payload=b'\0\x02'+bytes([width])+pattern
        cases.extend([stream(7,payload),stream(12,payload),dict(function=13,
            args=[OUTPUT,DATA,0,len(payload),1],setup=[(DATA,payload)])])
    dictionaries=[b'',b'\x8b\x00',encode(108)+encode(-1131)+b'\x12',
        b'\x1e\x12\x3f'+encode(777)+b'\x0c\x24',
        encode(-2147483648)+b'\x01'+encode(2147483647)+b'\x01',
        b'\x1c\x12\x34\x06\x1d\x12\x34\x56\x78\x0c\x05',b'\xff\x00']
    for payload in dictionaries:
        for key in (0,1,6,18,0x105,0x124,0xffffffff):
            for output in (OUTPUT,BUFFER,BUFFER+4):
                cases.append(stream(10,payload,extra=(key,),output=output))
            for count in (-1,0,1,2,8):
                cases.append(stream(11,payload,extra=(key,count,OUTPUT)))
    for function in (14,15,16,17):
        for value in (0,1,0x7f,0x80,0x7fff,0x8000,0xffff,0x7fffffff,0x80000000,0xffffffff):
            for offset in (0,1,3):
                length=2 if function<16 else 4
                raw=(value&((1<<(length*8))-1)).to_bytes(length,'big')
                result=int.from_bytes(raw,'big',signed=function in (15,17))&MASK
                cases.append(dict(function=function,args=[DATA+offset],setup=[(DATA+offset,raw)],expected_return=result))
    signatures=(b'1\0\0\0',b'typ1',b'OTTO',b'\0\x01\0\0',b'true')
    for signature in (*signatures,b'\0'*4,b'ttcf',b'OTTO'[:3]+b'\xff'):
        for function in (18,20,21):
            for index in (0,1,-1):
                args=[DATA,index] if function==20 else [DATA]
                value=(int(signature in signatures) if function in (18,21) else
                    0 if signature in signatures and index==0 else MASK)
                cases.append(dict(function=function,args=args,setup=[(DATA,signature+bytes(32))],expected_return=value))
    for version in (0,0x10000,0x20000,0x30000):
        for count in (0,1,3,0x80000000,0xffffffff):
            payload=b'ttcf'+struct.pack('>II',version,count)+struct.pack('>III',0x100,0x200,0x300)
            cases.append(dict(function=21,args=[DATA],setup=[(DATA,payload)],expected_return=count if version in (0x10000,0x20000) else 0))
            for index in (-3,-2,-1,0,1,2,3,0x80000000,0xffffffff):
                # The original permits negative indexes, even reading the
                # collection header; signed index/count comparison is retained.
                if index==0x80000000 and count not in (0x80000000,):continue
                cases.append(dict(function=20,args=[DATA,index],setup=[(DATA,payload)]))
    for offset in (0,64,0xffffffe0):
        for count in (0,1,3):
            payload=bytearray(12+count*16)
            struct.pack_into('>H',payload,4,count)
            for i in range(count):
                payload[12+i*16:16+i*16]=(b'head',b'cmap',b'\xffBAD')[i]
                struct.pack_into('>I',payload,20+i*16,0x12340000+i)
            for tag in (b'head',b'cmap',b'none',b'\xffBAD'):
                expected=(0x12340000 if tag==b'head' and count else 0x12340001 if tag==b'cmap' and count>=2 else 0)
                cases.append(dict(function=19,args=[DATA,offset,TAG],setup=[((DATA+offset)&MASK,bytes(payload)),(TAG,tag)],expected_return=expected))
    # Original shipped Fira TrueType font: every table entry plus endian
    # headers, followed by synthetic collections containing the real font.
    count=struct.unpack_from('>H',font,4)[0]
    cases.extend([dict(function=18,args=[FONT],expected_return=1),dict(function=21,args=[FONT],expected_return=1),
                  dict(function=20,args=[FONT,0],expected_return=0)])
    for i in range(count):
        tag,checksum,offset,length=struct.unpack_from('>4sIII',font,12+i*16)
        cases.append(dict(function=19,args=[FONT,0,TAG],setup=[(TAG,tag)],expected_return=offset))
        for function in (14,15,16,17):
            size=2 if function<16 else 4
            expected=int.from_bytes(font[offset:offset+size],'big',signed=function in (15,17))&MASK
            cases.append(dict(function=function,args=[FONT+offset],expected_return=expected))
    rng=random.Random(0x82b838)
    for _ in range(200):
        value=rng.randrange(-2147483648,2147483648)
        cases.append(dict(stream(8,encode(value)),expected_return=value&MASK))
        payload=bytes(rng.getrandbits(8) for _ in range(rng.randrange(1,65)))
        cases.append(stream(8,payload))
    # CFF private dictionary and local Subrs lookup. Both arguments are
    # by-value buffers, and original callees mutate those argument slots.
    for private_offset in (0,32,64):
        for private_size in (0,2,16):
            for subrs in (0,16,-1):
                font_data=bytearray(256)
                private=encode(subrs)+b'\x13'
                font_data[private_offset:private_offset+len(private)]=private
                dictionary=encode(private_size)+encode(private_offset)+b'\x12'
                if subrs>0:
                    index=index_data([b'abc',b'def'])
                    font_data[private_offset+subrs:private_offset+subrs+len(index)]=index
                cases.append(dict(function=22,args=[OUTPUT,DATA,77,len(font_data),TAG,99,len(dictionary)],
                    setup=[(DATA,bytes(font_data)),(TAG,dictionary)],expected_return=OUTPUT))
    for dictionary in (b'',b'\x8b\x12',encode(1)+encode(0x7fffffff)+b'\x12'):
        cases.append(dict(function=22,args=[OUTPUT,DATA,0,32,TAG,0,len(dictionary)],
            setup=[(DATA,bytes(32)),(TAG,dictionary)],expected_memory=[(OUTPUT,words(0,0,0))]))
    for cached in (0,1,0x7fffffff,0x80000000,0xffffffff):
        for present in (False,True):
            payload=bytearray(64)
            struct.pack_into('>H',payload,4,1)
            payload[12:16]=b'SVG ' if present else b'head'
            struct.pack_into('>I',payload,20,32)
            struct.pack_into('>I',payload,34,8)
            expected=(40 if present else 0) if cached>=0x80000000 else cached
            info=bytearray(48)
            struct.pack_into('<II',info,4,DATA,0)
            struct.pack_into('<I',info,44,cached)
            cases.append(dict(function=23,args=[BUFFER],setup=[(DATA,bytes(payload)),(BUFFER,bytes(info))],
                expected_return=expected,expected_memory=[(BUFFER+44,words(expected))]))
    return cases


def main():
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--installed',action='store_true')
    args=parser.parse_args()
    started=time.monotonic()
    source=(ROOT.parent/'IceStarOS_x86_32M.vhd').read_bytes()
    disk=Fat16(source);original=disk.read_kernel()
    assert hashlib.sha256(original).hexdigest()==ORIGINAL_SHA256
    font=disk.read_file(b'ICESTAR    ',b'FONT       ',b'FIRA    TTF')
    variants,sizes=compile_variants(original,args.installed)
    specs=[('original',original,BASE,[s[2] for s in SLOTS]),*variants]
    machines=[Machine(*s,original,font) for s in specs]
    cases=scenarios(font)
    for index,case in enumerate(cases):
        try:
            expected=machines[0].execute(case)
            for machine in machines[1:]:
                actual=machine.execute(case)
                assert actual==expected,(machine.label,expected,actual)
        except Exception:
            print('Failing case',index,{k:v for k,v in case.items() if k!='setup'},flush=True)
            raise
    summary=dict(installed=args.installed,functions=len(SLOTS),scenarios=len(cases),executions=len(cases)*3,
        slots=sizes,seconds=round(time.monotonic()-started,3),boundary_scope='No intercepted helper calls',
        checks=['return values','all touched data pages','callee-saved registers','RET 4 structure return ABI',
                'caller argument stack including by-value buffer mutation','original Fira font table directory'],
        original_font=dict(bytes=len(font),sha256=hashlib.sha256(font).hexdigest()),
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in
            ('kernel/font_parse.c','kernel/font_parse.h','tests/kernel_font_parse.py')})
    (BUILD/'font_parse_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2))


if __name__=='__main__':main()
