"""Quadratic closure, CFF/CID subroutines and pixel bounds: real x86/C parity."""
import argparse
import hashlib
import json
import math
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
from tests.kernel_font_cmap import (Machine as BaseMachine,words,STACK,STOP,START,CONTROL,MASK,SAVED,SENTINELS)
from tests.kernel_font_context import fbits,vertex_bytes
from tests.kernel_font_parse import index_data,encode
from tests.kernel_font_glyph import real_info,font_info
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EFLAGS,UC_X86_REG_EIP,UC_X86_REG_FPCW)
BASE,DATA,BUFFER,OUTPUT,FONT=0x800000,0x600000,0x500000,0x510000,0xa00000
SLOTS=[('font_close_shape','kernel_font_close_shape_82d24a',0x82d24a,0x82d386),
 ('font_subroutine','kernel_font_subroutine_82e4ac',0x82e4ac,0x82e552),
 ('font_cid_subrs','kernel_font_cid_subrs_82e552',0x82e552,0x82e6f0),
 ('font_bitmap_box_subpixel','kernel_font_bitmap_box_subpixel_8308d2',0x8308d2,0x830a3c),
 ('font_bitmap_box','kernel_font_bitmap_box_830a3c',0x830a3c,0x830a92),
 ('font_codepoint_bitmap_box_subpixel','kernel_font_codepoint_bitmap_box_subpixel_830a92',0x830a92,0x830af6),
 ('font_codepoint_bitmap_box','kernel_font_codepoint_bitmap_box_830af6',0x830af6,0x830b4c)]
AGGREGATE={1,2}


def compile_variants(original,installed,module="outline",slots=SLOTS):
    obj=BUILD/f'kernel_font_{module}_preview.o'
    run(['gcc',*FLAGS,'-fexcess-precision=fast','-c',f'kernel/font_{module}.c','-o',str(obj)])
    undefined=run(['nm','-u',str(obj)],capture_output=True,text=True).stdout.splitlines()
    provides=''.join(f'PROVIDE({s} = 0x{s.rsplit("_",1)[1]});\n' for s in (line.split()[-1] for line in undefined))
    outputs=[];sizes={}
    for fixed in (False,True):
        label='fixed' if fixed else 'ordinary';script,elf=BUILD/f'font_{module}_{label}.ld',BUILD/f'font_{module}_{label}.elf'
        sections='\n'.join(f'.{n} {s:#x} : {{ *(.text.{sym}) }}\n'
            f'ASSERT({sym}=={s:#x} && SIZEOF(.{n})<={e-s}, "outline overflow")'
            for n,sym,s,e in slots) if fixed else '. = 0x300000; .text : { *(.text*) }'
        script.write_text(f'OUTPUT_FORMAT(elf32-i386)\nENTRY({slots[0][1]})\n'+provides+
            'SECTIONS {\n'+sections+'\n/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n'
            '.unexpected : { *(*) }\nASSERT(SIZEOF(.unexpected)==0, "unexpected section")\n}\n')
        run(['ld','-m','elf_i386','-T',str(script),'-o',str(elf),str(obj)])
        symbols={line.split()[-1]:int(line.split()[0],16) for line in run(['nm',str(elf)],capture_output=True,text=True).stdout.splitlines()}
        if fixed:
            candidate=bytearray(original)
            for n,sym,s,e in slots:
                out=BUILD/f'font_{module}_preview.{n}.bin'
                run(['objcopy','-O','binary','-j','.'+n,str(elf),str(out)])
                body=out.read_bytes();assert 0<len(body)<=e-s
                candidate[s-BASE:e-BASE]=body+b'\x90'*(e-s-len(body));sizes[n]=dict(compiled_bytes=len(body),slot_bytes=e-s)
            if installed:
                production=(BUILD/'openicestaros_kernel.x').read_bytes()
                for _,_,s,e in slots:assert production[s-BASE:e-BASE]==candidate[s-BASE:e-BASE]
                candidate=production
            outputs.append((label,bytes(candidate),BASE,[symbols[sym] for _,sym,_,_ in slots]))
        else:
            out=BUILD/f'font_{module}_ordinary.bin';run(['objcopy','-O','binary',str(elf),str(out)])
            outputs.append((label,out.read_bytes(),0x300000,[symbols[sym] for _,sym,_,_ in slots]))
    return outputs,sizes


class Machine(BaseMachine):
    aggregates=AGGREGATE
    returns={0,1,2}
    mutable_caller={0,1}
    def execute(self,case):
        uc=self.uc
        stack_base=STACK+case.get("stack_offset",0)
        for page in self.old_pages:uc.mem_write(page,bytes(4096))
        self.pages,self.done,self.fault=set(),False,None
        self.put(OUTPUT,bytes([0xa5])*256)
        for address,data in case.get('setup',[]):self.put(address,data)
        arguments=[x&MASK for x in case['args']];fn=case['function']
        uc.mem_write(CONTROL,struct.pack('<H',case.get('control',0x37f)))
        entry=self.entries[fn]
        uc.mem_write(START,b'\xdb\xe3\xd9\x2d'+struct.pack('<I',CONTROL)+b'\xe9'+struct.pack('<I',(entry-START-13)&MASK))
        uc.mem_write(STOP,b'\x90'*12)
        uc.ctl_remove_cache(START,START+16);uc.ctl_remove_cache(STOP,STOP+16)
        stack=struct.pack('<'+'I'*(len(arguments)+1),STOP,*arguments)
        uc.mem_write(STACK-0x4000,bytes(0x5000));uc.mem_write(stack_base,stack)
        uc.reg_write(UC_X86_REG_ESP,stack_base);uc.reg_write(UC_X86_REG_EFLAGS,2)
        for reg,value in zip(SAVED,SENTINELS):uc.reg_write(reg,value)
        uc.emu_start(START,0,count=2000000)
        assert self.done,(self.label,case,hex(uc.reg_read(UC_X86_REG_EIP)))
        assert uc.reg_read(UC_X86_REG_ESP)==stack_base+4+(4 if fn in self.aggregates else 0),'aggregate RET 4'
        assert tuple(uc.reg_read(reg) for reg in SAVED)==SENTINELS,'callee-saved registers'
        assert uc.reg_read(UC_X86_REG_FPCW)==case.get('control',0x37f),'x87 control changed'
        caller=bytes(uc.mem_read(stack_base+4,len(arguments)*4))
        if 'expected_caller' in case:assert caller==case['expected_caller'],(self.label,'caller',caller,case['expected_caller'])
        if fn not in self.mutable_caller and not case.get('caller_alias'):assert caller==stack[4:],'caller arguments changed'
        result=uc.reg_read(UC_X86_REG_EAX) if fn in self.returns else None
        if 'expected_return' in case:assert result==case['expected_return']&MASK,(self.label,result,case['expected_return'])
        for address,data in case.get('expected_memory',[]):
            actual=bytes(uc.mem_read(address,len(data)))
            assert actual==data,(self.label,hex(address),actual,data)
        if case.get('stack_alias'):caller+=bytes(uc.mem_read(stack_base+44,64))
        memory=[(page,hashlib.sha256(uc.mem_read(page,4096)).hexdigest()) for page in sorted(self.pages)]
        self.old_pages=self.pages
        return result,caller,memory


def signed(v):return (v&0x7fffffff)-(v&0x80000000)
def fixture(fn,args,setup=(),expected=None,memory=(),control=0x37f):
    c=dict(function=fn,args=args,setup=list(setup),expected_memory=list(memory),control=control)
    if expected is not None:c['expected_return']=expected
    return c


def contour_cases():
    rng=random.Random(0x82d24a);cases=[]
    values=(-2147483648,-65536,-32769,-1,0,1,32767,65535,2147483647)
    for off in (0,1,-1,2):
        for start in (0,1,-1,2):
            for count in (0,1,0xffffffff,0x7fffffff,0x80000000):
                for i in range(12):
                    sx,sy,scx,scy,cx,cy=[rng.choice(values) for _ in range(6)]
                    vertices=[]
                    if start:
                        if off:vertices.append((3,signed(cx+scx)>>1,signed(cy+scy)>>1,cx,cy))
                        vertices.append((3,sx,sy,scx,scy))
                    else:vertices.append((3 if off else 2,sx,sy,cx if off else 0,cy if off else 0))
                    raw=bytearray([0xa5])*(len(vertices)*14)
                    for j,v in enumerate(vertices):
                        packed=vertex_bytes(*v);raw[j*14:j*14+8]=packed[:8];raw[j*14+12]=packed[12]
                    args=[(OUTPUT-count*14)&MASK,count,off,start,sx,sy,scx,scy,cx,cy]
                    c=fixture(0,args,expected=(count+len(vertices))&MASK,memory=[(OUTPUT,bytes(raw))])
                    c['expected_caller']=words(args[0],count+len(vertices),*args[2:]);cases.append(c)
    for slot in (4,5,6,7,8,9):
        c=fixture(0,[STACK+4+slot*4,0,1,1,10,20,30,40,50,60],expected=2)
        c['stack_alias']=True;cases.append(c)
    return cases


def subroutine_cases():
    cases=[]
    for count in (0,1,7,1239,1240,33899,33900,65535):
        bias=32768 if count>33899 else 1131 if count>1239 else 107
        for width in (1,2,3,4):
            if count+1>=1<<(width*8):continue
            payload=index_data([bytes([i&255]) for i in range(count)],width)
            for selected in (-1,0,1,count-1,count,0x7fffffff,-2147483648):
                index=signed(selected-bias)
                for output in (OUTPUT,BUFFER):
                    args=[output,DATA,19,len(payload),index]
                    memory=[(output,words(0,0,0))] if not 0<=signed(selected)<count else [
                        (output,words(DATA+3+(count+1)*width+selected,0,1))]
                    c=fixture(1,args,[(DATA,payload)],output,memory)
                    c['expected_caller']=words(output,DATA,2,len(payload),selected)
                    cases.append(c)
    for payload in (b'',b'\0',b'\0\x02',b'\0\x02\1\1\2\3AB'):
        for index in (-108,-107,-106,0,2147483647):
            cases.append(fixture(1,[OUTPUT,DATA,23,len(payload),index],[(DATA,payload)],OUTPUT))
    return cases


def cid_fixture(select,glyph,size=None,output=OUTPUT):
    raw=bytearray(0x2000);dictionaries=[];subr=[]
    for fd in range(3):
        private=256+fd*64;dictionaries.append(encode(2)+encode(private)+b'\x12')
        raw[private:private+2]=encode(8)+b'\x13';stream=index_data([bytes([fd+1,11])])
        raw[private+8:private+8+len(stream)]=stream;subr.append((DATA+private+8,0,len(stream)))
    array=index_data(dictionaries);raw[0x1000:0x1000+len(array)]=array
    raw[0x1800:0x1800+len(select)]=select;info=bytearray(160)
    struct.pack_into('<3I',info,56,DATA,0,len(raw));struct.pack_into('<3I',info,104,DATA+0x1000,13,len(array))
    struct.pack_into('<3I',info,116,DATA+0x1800,11,len(select) if size is None else size)
    return fixture(2,[output,BUFFER,glyph],[(DATA,bytes(raw)),(BUFFER,bytes(info))],output),subr


def cid_cases():
    cases=[];select=b'\0\0\1\2\1\0'
    for glyph in (-1,0,1,2,3,4,5,65535,0x7fffffff,-2147483648):
        for output in (OUTPUT,BUFFER,BUFFER+56,BUFFER+104,BUFFER+116):
            c,subr=cid_fixture(select,glyph,output=output)
            if 0<=glyph<5:c['expected_memory']=[(output,words(*subr[select[1+glyph]]))]
            cases.append(c)
    ranges=b'\3'+struct.pack('>HHBHBHBH',3,0,2,3,1,8,0,12)
    for glyph in (-1,0,2,3,7,8,11,12,65535,0x7fffffff,-2147483648):
        c,subr=cid_fixture(ranges,glyph)
        if 0<=glyph<12:c['expected_memory']=[(OUTPUT,words(*subr[2 if glyph<3 else 1 if glyph<8 else 0]))]
        cases.append(c)
    for select in (b'',b'\1',b'\2',b'\4',b'\xff',ranges,select):
        for size in range(len(select)+1):
            for glyph in (0,1,4,13):cases.append(cid_fixture(select,glyph,size)[0])
    return cases


def bbox_case(fn,info,setup,glyph=0,scale=(0.5,0.25),shift=(0.75,-0.5),outputs=None,box=None,control=0x37f):
    if outputs is None:outputs=[OUTPUT+i*4 for i in range(4)]
    shifted=fn in (3,5);dx,dy=shift if shifted else (0.,0.)
    args=[BUFFER,glyph,*[fbits(v) for v in scale],*([fbits(dx),fbits(dy)] if shifted else []),*outputs]
    memory=[]
    if box is not None:
        x0,y0,x1,y1=box;sx,sy=scale
        expected=(math.floor(x0*sx+dx),math.floor(-y1*sy+dy),math.ceil(x1*sx+dx),math.ceil(-y0*sy+dy))
        expected_bytes={}
        for p,v in zip(outputs,expected):
            if p:
                for j,b in enumerate(words(v)):expected_bytes[p+j]=b
        memory=[(p,bytes([b])) for p,b in expected_bytes.items()]
    return fixture(fn,args,[(BUFFER,info),*setup],memory=memory,control=control)


def bbox_cases(font):
    cases=[];raw=bytearray(0x1000);struct.pack_into('>3H',raw,128,128,138,138)
    struct.pack_into('>5h',raw,512,1,-123,-32768,32767,456)
    struct.pack_into('>HHH',raw,1024,0,262,0);raw[1030+65]=0;raw[1030+66]=1
    info=font_info(glyphs=2,loca=128,glyf=256,loca_format=0,cmap=1024)
    for fn in range(3,7):
        for mask in range(16):
            for scale in ((1.,1.),(.5,.25),(-.5,-.25),(0.,0.),(.75,-.5)):
                output=[OUTPUT+i*4 if mask&(1<<i) else 0 for i in range(4)]
                for glyph in (0,1):
                    box=(-123,-32768,32767,456) if not glyph else (0,0,0,0)
                    c=bbox_case(fn,info,[(DATA,bytes(raw))],glyph=(65+glyph if fn>=5 else glyph),scale=scale,outputs=output,box=box)
                    if glyph:c['expected_memory']=[(p,words(0)) for p in output if p]
                    cases.append(c)
        for output in ([OUTPUT]*4,[OUTPUT,OUTPUT+1,OUTPUT+2,OUTPUT+3],[BUFFER+4,BUFFER+24,DATA+514,DATA+516]):
            cases.append(bbox_case(fn,info,[(DATA,bytes(raw))],glyph=65 if fn>=5 else 0,outputs=output,box=(-123,-32768,32767,456)))
        for control in (0x37f,0x7f,0x27f,0x77f,0xb7f,0xf7f):
            for value in (0.,-0.,.1,-.1,1e-40,float('inf'),float('-inf'),float('nan'),2147483648.):
                cases.append(bbox_case(fn,info,[(DATA,bytes(raw))],glyph=65 if fn>=5 else 0,scale=(value,-value),control=control))
    for slot in (2,3,4,5,7,8,9):
        c=bbox_case(3,info,[(DATA,bytes(raw))],outputs=[STACK+4+slot*4,OUTPUT+4,OUTPUT+8,OUTPUT+12])
        c['caller_alias']=True;cases.append(c)
    info,tables,count,fmt,selected=real_info(font)
    offsets=[struct.unpack_from('>H' if fmt==0 else '>I',font,tables[b'loca']+i*(2 if fmt==0 else 4))[0]*(2 if fmt==0 else 1) for i in range(count+1)]
    for glyph in range(count):
        empty=offsets[glyph]==offsets[glyph+1]
        box=(0,0,0,0) if empty else struct.unpack_from('>4h',font,tables[b'glyf']+offsets[glyph]+2)
        c=bbox_case(3 if glyph&1 else 4,info,[],glyph,scale=(.03125,.0625),box=box)
        if empty:c['expected_memory']=[(OUTPUT,words(0,0,0,0))]
        cases.append(c)
    program=bytes([139,139,21,149,159,169,179,189,199,8,14])
    index=index_data([program]);info=bytearray(font_info(cff_size=1));struct.pack_into('<3I',info,68,DATA,0,len(index))
    for fn in (3,4):cases.append(bbox_case(fn,bytes(info),[(DATA,index)],box=(0,0,90,120)))
    for c in cases[::17]:c['stack_offset']=4
    return cases


def main():
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--installed',action='store_true');args=parser.parse_args()
    started=time.monotonic();disk=Fat16((ROOT.parent/'IceStarOS_x86_32M.vhd').read_bytes());original=disk.read_kernel()
    assert hashlib.sha256(original).hexdigest()==ORIGINAL_SHA256
    font=disk.read_file(b'ICESTAR    ',b'FONT       ',b'FIRA    TTF');variants,sizes=compile_variants(original,args.installed)
    machines=[Machine(*spec,original,font) for spec in [('original',original,BASE,[s for _,_,s,_ in SLOTS]),*variants]]
    cases=contour_cases()+subroutine_cases()+cid_cases()+bbox_cases(font);counts={n:0 for n,_,_,_ in SLOTS}
    for i,c in enumerate(cases):
        try:
            expected=machines[0].execute(c)
            for m in machines[1:]:
                actual=m.execute(c);assert actual==expected,(m.label,expected,actual)
        except Exception:
            print('Failing outline case',i,{k:v for k,v in c.items() if k!='setup'},flush=True);raise
        counts[SLOTS[c['function']][0]]+=1
    summary=dict(installed=args.installed,functions=len(SLOTS),scenarios=len(cases),executions=3*len(cases),slots=sizes,
        scenarios_by_function=counts,seconds=round(time.monotonic()-started,3),
        checks=['all touched data pages','caller-area count/cursor/bias writes','aggregate RET 4','callee-saved registers',
            'six preserved x87 controls','independent packed quadratic vertices','all Fira glyph bounds',
            'CFF subroutine bias thresholds','CID formats 0/3 and partial data','output aliases'],
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ('kernel/font_outline.c','kernel/font_outline.h','tests/kernel_font_outline.py')})
    (BUILD/'font_outline_summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))

if __name__=='__main__':main()
