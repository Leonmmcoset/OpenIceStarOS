"""Compare nine font metric/scaling/SVG functions against original x86.

The ordinary version and installed kernel both execute all nested helpers.
All helper calls execute; floating results compare raw x87 80-bit values
and their 32-bit conversions under multiple precision/rounding controls.
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
FLOAT={4,5}
VOID={0,1,3}
F32,F80,CONTROL,START=0x530000,0x530010,0x530020,0x210000
SLOTS = [
    ('font_hmetrics', 'kernel_font_hmetrics_82f958', 0x82f958, 0x82fa50),
    ('font_vmetrics', 'kernel_font_vmetrics_83054a', 0x83054a, 0x8305c0),
    ('font_typo_metrics', 'kernel_font_typo_metrics_8305c0', 0x8305c0, 0x83066a),
    ('font_bounds', 'kernel_font_bounds_83066a', 0x83066a, 0x8306ec),
    ('font_scale_height', 'kernel_font_scale_height_8306ec', 0x8306ec, 0x83073a),
    ('font_scale_em', 'kernel_font_scale_em_83073a', 0x83073a, 0x830768),
    ('font_svg_record', 'kernel_font_svg_record_83077c', 0x83077c, 0x83081c),
    ('font_glyph_svg', 'kernel_font_glyph_svg_83081c', 0x83081c, 0x8308a0),
    ('font_codepoint_svg', 'kernel_font_codepoint_svg_8308a0', 0x8308a0, 0x8308d2),
]


def compile_variants(original, installed):
    obj = BUILD / 'kernel_font_metrics_preview.o'
    run(['gcc', *FLAGS, '-c', 'kernel/font_metrics.c', '-o', str(obj)])
    outputs, sizes = [], {}
    for fixed in (False, True):
        label = 'fixed' if fixed else 'ordinary'
        script, elf = BUILD/f'font_metrics_{label}.ld', BUILD/f'font_metrics_{label}.elf'
        sections = '\n'.join(
            f'.{name} {start:#x} : {{ *(.text.{symbol}) }}\n'
            f'ASSERT({symbol} == {start:#x} && SIZEOF(.{name}) <= {end-start}, "slot overflow")'
            for name, symbol, start, end in SLOTS) if fixed else '. = 0x300000; .text : { *(.text*) }'
        script.write_text('OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_font_hmetrics_82f958)\n'
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
                path = BUILD/f'font_metrics_preview.{name}.bin'
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
            path = BUILD/'font_metrics_ordinary.bin'
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
        uc.emu_start(START,0,count=2000000)
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

def metrics_case(function,arguments=(),values=(800,-200,50),outputs=(),**kwargs):
    payload=bytearray(4096)
    struct.pack_into('>hhh',payload,256+4,*values)
    struct.pack_into('>H',payload,256+34,3)
    struct.pack_into('>H',payload,128+18,1000)
    struct.pack_into('>hhhh',payload,128+36,-30,-400,900,1100)
    for i in range(8):struct.pack_into('>hh',payload,512+4*i,500+i,10+i)
    info=bytearray(64)
    struct.pack_into('<I',info,4,DATA)
    struct.pack_into('<I',info,20,128)
    struct.pack_into('<I',info,28,256)
    struct.pack_into('<I',info,32,512)
    return dict(function=function,args=[BUFFER,*arguments,*outputs],
        setup=[(DATA,bytes(payload)),(BUFFER,bytes(info))],**kwargs)


def scenarios(font):
    cases=[]
    for function in (0,1,2,3):
        combinations=16 if function==3 else 8
        for mask in range(combinations):
            for values in ((800,-200,50),(32767,-32768,-1),(-32768,32767,0),(0,0,0)):
                count=4 if function==3 else 3 if function in (1,2) else 2
                outputs=[OUTPUT+i*4 if mask&(1<<i) or function==3 else 0 for i in range(count)]
                for glyph in ((-1,0,2,3,4,7,0xffffffff) if function==0 else (0,)):
                    cases.append(metrics_case(function,(glyph,) if function==0 else (),values,outputs))
    # Output writes may change the font data pointer or table offsets before
    # later outputs read them. Every output rereads the relevant live fields.
    for function in (0,1,2,3):
        for offset in (4,20,28,32):
            count=4 if function==3 else 3 if function in (1,2) else 2
            outputs=[BUFFER+offset]+[OUTPUT+i*4 for i in range(1,count)]
            cases.append(metrics_case(function,(1,) if function==0 else (),outputs=outputs))
        cases.append(metrics_case(function,(1,) if function==0 else (),outputs=[OUTPUT]*count))
    for count in (0,1,2,3,65535):
        for glyph in (-1,0,1,2,3,10,0x7fffffff):
            case=metrics_case(0,(glyph,),outputs=(OUTPUT,OUTPUT+4))
            case['setup'].append((DATA+256+34,struct.pack('>H',count)))
            cases.append(case)
    for present in (False,True):
        for mask in range(8):
            payload=bytearray(4096)
            struct.pack_into('>H',payload,4,1)
            payload[12:16]=b'OS/2' if present else b'head'
            struct.pack_into('>I',payload,20,1024)
            struct.pack_into('>hhh',payload,1024+68,700,-300,99)
            case=metrics_case(2,outputs=[OUTPUT+i*4 if mask&(1<<i) else 0 for i in range(3)],expected_return=int(present))
            case['setup'].append((DATA,bytes(payload)))
            cases.append(case)
    for function in (4,5):
        for control in (0x37f,0x7f,0x27f,0x77f,0xb7f,0xf7f):
            for bits in (0,0x80000000,0x3f800000,0x41c00000,0x41880000,1,0x80000001,
                         0x7f800000,0xff800000,0x7fc12345,0x7f812345,0x7f7fffff):
                for values in ((800,-200,50),(0,0,0),(-200,800,0),(32767,-32768,0)):
                    case=metrics_case(function,(bits,),values,control=control)
                    if function==5 and values==(0,0,0):case['setup'].append((DATA+128+18,b'\0\0'))
                    cases.append(case)
    # SVG list selection uses inclusive signed glyph bounds. A zero cache
    # suppresses document access before lookup; negative cache triggers lookup.
    for cache in (0,64,0xffffffff):
        for glyph in (-1,0,1,2,3,4,5,65535,0x7fffffff):
            for function in (6,7,8):
                payload=bytearray(512)
                struct.pack_into('>H',payload,4,1)
                payload[12:16]=b'SVG '
                struct.pack_into('>I',payload,20,32)
                struct.pack_into('>I',payload,34,32)
                struct.pack_into('>H',payload,64,2)
                struct.pack_into('>HHII',payload,66,1,3,100,10)
                struct.pack_into('>HHII',payload,78,5,8,120,20)
                # Original cmap format 0: map codepoints 0..255 to themselves.
                struct.pack_into('>HHH',payload,256,0,262,0)
                payload.extend(bytes(6))
                for i in range(256):
                    if 262+i>=len(payload):payload.extend(bytes(262+i-len(payload)+1))
                    payload[262+i]=i
                info=bytearray(64)
                struct.pack_into('<I',info,4,DATA)
                struct.pack_into('<I',info,44,cache)
                struct.pack_into('<I',info,48,256)
                # Keep original format-0 lookup within its declared size.
                if function==8 and glyph<0:continue
                cases.append(dict(function=function,args=[BUFFER,glyph]+([OUTPUT] if function in (7,8) else []),
                    setup=[(DATA,bytes(payload)),(BUFFER,bytes(info))]))
    # Original Fira font: parse expected metrics directly from its table bytes.
    tables={}
    for i in range(struct.unpack_from('>H',font,4)[0]):
        tag,checksum,offset,length=struct.unpack_from('>4sIII',font,12+i*16)
        tables[tag]=offset
    info=bytearray(64)
    for offset,value in ((4,FONT),(20,tables[b'head']),(28,tables[b'hhea']),(32,tables[b'hmtx']),(44,0)):
        struct.pack_into('<I',info,offset,value)
    setup=[(BUFFER,bytes(info))]
    vmetrics=struct.unpack_from('>hhh',font,tables[b'hhea']+4)
    cases.append(dict(function=1,args=[BUFFER,OUTPUT,OUTPUT+4,OUTPUT+8],setup=setup,
        expected_memory=[(OUTPUT,words(*vmetrics))]))
    cases.append(dict(function=2,args=[BUFFER,OUTPUT,OUTPUT+4,OUTPUT+8],setup=setup,expected_return=1,
        expected_memory=[(OUTPUT,words(*struct.unpack_from('>hhh',font,tables[b'OS/2']+68)))]))
    cases.append(dict(function=3,args=[BUFFER,OUTPUT,OUTPUT+4,OUTPUT+8,OUTPUT+12],setup=setup,
        expected_memory=[(OUTPUT,words(*struct.unpack_from('>hhhh',font,tables[b'head']+36)))]))
    count=struct.unpack_from('>H',font,tables[b'hhea']+34)[0]
    glyph_count=struct.unpack_from('>H',font,tables[b'maxp']+4)[0]
    for glyph in sorted(set([0,1,2,32,65,128,count-1,count,count+1,glyph_count-1])):
        advance=struct.unpack_from('>h',font,tables[b'hmtx']+4*min(glyph,count-1))[0]
        bearing_offset=4*glyph+2 if glyph<count else 2*(count+glyph)
        bearing=struct.unpack_from('>h',font,tables[b'hmtx']+bearing_offset)[0]
        cases.append(dict(function=0,args=[BUFFER,glyph,OUTPUT,OUTPUT+4],setup=setup,
            expected_memory=[(OUTPUT,words(advance,bearing))]))
    for pixels in (1.0,17.0,24.0,64.0):
        for function,denominator in ((4,vmetrics[0]-vmetrics[1]),
            (5,struct.unpack_from('>H',font,tables[b'head']+18)[0])):
            bits=struct.unpack('<I',struct.pack('<f',pixels))[0]
            cases.append(dict(function=function,args=[BUFFER,bits],setup=setup,expected_float=pixels/denominator))
    return cases


def main():
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--installed',action='store_true')
    args=parser.parse_args();started=time.monotonic()
    disk=Fat16((ROOT.parent/'IceStarOS_x86_32M.vhd').read_bytes())
    original=disk.read_kernel()
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
        slots=sizes,seconds=round(time.monotonic()-started,3),boundary_scope='No intercepted helper calls; real original cmap lookup',
        checks=['returns','all touched data pages','caller arguments','callee-saved registers',
                'raw x87 extended results and float32 conversions','six x87 control words','original Fira metrics'],
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in
                ('kernel/font_metrics.c','kernel/font_metrics.h','tests/kernel_font_metrics.py')})
    (BUILD/'font_metrics_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2))


if __name__=='__main__':main()
