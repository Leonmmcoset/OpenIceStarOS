"""Original/ordinary/fixed CFF context, x87 rounding, output and bounds."""
import argparse
import hashlib
import json
import random
import struct
import sys
import time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(BUILD/'test-deps')]
from tools.kernel_image import Fat16,ORIGINAL_SHA256
from tests.kernel_x_loader import FLAGS,run
from tests.kernel_font_cmap import Machine as BaseMachine,words
from unicorn.x86_const import UC_X86_REG_FPCW
BASE,DATA,BUFFER,OUTPUT,FONT=0x800000,0x600000,0x500000,0x510000,0xa00000
SLOTS=[('font_context_bounds','kernel_font_context_bounds_82e05c',0x82e05c,0x82e0e0),
 ('font_context_vertex','kernel_font_context_vertex_82e0e0',0x82e0e0,0x82e1d6),
 ('font_context_close','kernel_font_context_close_82e1d6',0x82e1d6,0x82e27c),
 ('font_context_move','kernel_font_context_move_82e27c',0x82e27c,0x82e334),
 ('font_context_line','kernel_font_context_line_82e334',0x82e334,0x82e3c8),
 ('font_context_curve','kernel_font_context_curve_82e3c8',0x82e3c8,0x82e4ac)]


def compile_variants(original,installed):
    obj=BUILD/'kernel_font_context_preview.o';run(['gcc',*FLAGS,'-c','kernel/font_context.c','-o',str(obj)])
    outputs=[];sizes={}
    for fixed in (False,True):
        label='fixed' if fixed else 'ordinary';script,elf=BUILD/f'font_context_{label}.ld',BUILD/f'font_context_{label}.elf'
        section='\n'.join(f'.{n} {s:#x} : {{ *(.text.{symbol}) }}\n'
            f'ASSERT({symbol}=={s:#x} && SIZEOF(.{n})<={e-s}, "context overflow")'
            for n,symbol,s,e in SLOTS) if fixed else '. = 0x300000; .text : { *(.text*) }'
        script.write_text('OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_font_context_bounds_82e05c)\n'
            'PROVIDE(kernel_font_vertex_82cf10 = 0x82cf10);\nSECTIONS {\n'+section+
            '\n/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n.unexpected : { *(*) }\n'
            'ASSERT(SIZEOF(.unexpected)==0, "unexpected section")\n}\n')
        run(['ld','-m','elf_i386','-T',str(script),'-o',str(elf),str(obj)])
        symbols={line.split()[-1]:int(line.split()[0],16) for line in run(['nm',str(elf)],capture_output=True,text=True).stdout.splitlines()}
        if fixed:
            candidate=bytearray(original)
            for n,symbol,s,e in SLOTS:
                out=BUILD/f'font_context_preview.{n}.bin'
                run(['objcopy','-O','binary','-j','.'+n,str(elf),str(out)])
                body=out.read_bytes();assert 0<len(body)<=e-s
                candidate[s-BASE:e-BASE]=body+b'\x90'*(e-s-len(body));sizes[n]=dict(compiled_bytes=len(body),slot_bytes=e-s)
            if installed:
                production=(BUILD/'openicestaros_kernel.x').read_bytes()
                for _,_,s,e in SLOTS:assert production[s-BASE:e-BASE]==candidate[s-BASE:e-BASE]
                candidate=production
            outputs.append((label,bytes(candidate),BASE,[symbols[s] for _,s,_,_ in SLOTS]))
        else:
            out=BUILD/'font_context_ordinary.bin';run(['objcopy','-O','binary',str(elf),str(out)])
            outputs.append((label,out.read_bytes(),0x300000,[symbols[s] for _,s,_,_ in SLOTS]))
    return outputs,sizes


class Machine(BaseMachine):
    def written(self,uc,access,address,size,value,unused):
        if address<0x1000000 or address>=0xffff0000:
            super().written(uc,access,address,size,value,unused)

    def execute(self,case):
        result=super().execute(case)
        assert self.uc.reg_read(UC_X86_REG_FPCW)==case.get('control',0x37f),'x87 control word changed'
        if result[0]=='fault':
            memory=[(page,hashlib.sha256(self.uc.mem_read(page,4096)).hexdigest()) for page in sorted(self.pages)]
            return (*result,memory)
        return (None,*result[1:])


def fbits(value):return struct.unpack('<I',struct.pack('<f',value))[0]
def context(mode=1,started=0,sx=0.,sy=0.,x=0.,y=0.,bounds=(0,0,0,0),output=OUTPUT,count=0):
    return words(mode,started,fbits(sx),fbits(sy),fbits(x),fbits(y),*bounds,output,count)


def fixture(fn,args,raw,control=0x37f,memory=()):
    return dict(function=fn,args=[BUFFER,*args],control=control,
        setup=[(BUFFER,raw)],expected_memory=list(memory))


def vertex_bytes(type,x,y,cx=0,cy=0,cx1=0,cy1=0):
    return struct.pack('<6HB',*(v&65535 for v in (x,y,cx,cy,cx1,cy1)),type&255)


def scenarios():
    cases=[];rng=random.Random(0x82e05c)
    extremes=(-2147483648,-65536,-32769,-1,0,1,32767,65535,2147483647)
    for started in (0,1,2,0xffffffff):
        for x in extremes:
            for y in extremes:
                raw=context(started=started,bounds=(-100,100,-200,200))
                expected=(min(-100,x),max(100,x),min(-200,y),max(200,y)) if started else (x,x,y,y)
                cases.append(fixture(0,[x,y],raw,memory=[(BUFFER+24,words(*expected)),(BUFFER+4,words(1))]))
    for mode in (0,1,2,0xffffffff):
        for type in (0,1,2,3,4,0x104,0xff):
            for i in range(16):
                values=[rng.choice(extremes) for _ in range(6)]
                raw=context(mode=mode,started=i&1,bounds=(-100,100,-200,200),count=i%4)
                cases.append(fixture(1,[type,*values],raw))
    controls=(0x37f,0x7f,0x27f,0x77f,0xb7f,0xf7f)
    floats=[0.,-0.,0.5,-0.5,1.25,-1.25,32767.9,-32768.9,65536.,1e-40,
        float('inf'),float('-inf'),struct.unpack('<f',struct.pack('<I',0x7fc12345))[0],
        struct.unpack('<f',struct.pack('<I',0x7f812345))[0],2147483648.,-2147483648.]
    for control in controls:
        for mode in (0,1):
            for i,x in enumerate(floats):
                y=floats[(i+3)%len(floats)]
                for equal in (True,False):
                    raw=context(mode=mode,started=1,sx=x,sy=y,x=x if equal else 123.,y=y if equal else -456.,
                        bounds=(-100,100,-200,200))
                    cases.append(fixture(2,[],raw,control))
                raw=context(mode=mode,sx=1.5,sy=-2.25,x=x,y=y)
                for fn in (3,4):cases.append(fixture(fn,[fbits(0.75),fbits(-0.5)],raw,control))
                cases.append(fixture(5,[fbits(v) for v in (0.75,-0.5,x,y,1.25,-1.5)],raw,control))
    # Known geometry independently specifies all vertex and context fields.
    for fn,args,expected_xy,type,controls_xy in [(3,[3.,4.],(3.,4.),1,(0,0,0,0)),
        (4,[3.,4.],(3.,4.),2,(0,0,0,0)),
        (5,[1.,2.,3.,4.,5.,6.],(9.,12.),4,(1,2,4,6))]:
        for mode in (0,1):
            raw=context(mode=mode)
            memory=[(BUFFER+16,words(*(fbits(x) for x in expected_xy))),(BUFFER+44,words(1))]
            if not mode:memory.append((OUTPUT,vertex_bytes(type,*map(int,expected_xy),*controls_xy)))
            cases.append(fixture(fn,[fbits(v) for v in args],raw,memory=memory))
    # The first writer may mutate the context used by later field stores.
    for address in (BUFFER,BUFFER+4,BUFFER+8,BUFFER+24,BUFFER+28,BUFFER+40):
        raw=context(mode=0,output=address,count=0)
        c=fixture(1,[4,17,-23,31,-41,53,-67],raw)
        if address==BUFFER+40:
            # Vertex endpoint overwrites the live output pointer, while its
            # control point overwrites the live vertex count before cx1.
            from unicorn import UC_MEM_WRITE_UNMAPPED
            c['expected_fault']=(UC_MEM_WRITE_UNMAPPED,(0xffe90011+0xffd7001f*14+8)&0xffffffff,2)
        cases.append(c)
    # Wrapped vertex indices still address the specified output memory.
    for count in (0xffffffff,0x80000000,0x7fffffff):
        pointer=(OUTPUT-count*14)&0xffffffff
        raw=context(mode=0,output=pointer,count=count)
        cases.append(fixture(1,[4,1,2,3,4,5,6],raw,memory=[(OUTPUT,vertex_bytes(4,1,2,3,4,5,6))]))
    return cases


def main():
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--installed',action='store_true');args=parser.parse_args()
    started=time.monotonic();disk=Fat16((ROOT.parent/'IceStarOS_x86_32M.vhd').read_bytes());original=disk.read_kernel()
    assert hashlib.sha256(original).hexdigest()==ORIGINAL_SHA256
    variants,sizes=compile_variants(original,args.installed)
    machines=[Machine(*spec,original,b'') for spec in [('original',original,BASE,[s for _,_,s,_ in SLOTS]),*variants]]
    cases=scenarios();counts={n:0 for n,_,_,_ in SLOTS}
    for i,c in enumerate(cases):
        try:
            expected=machines[0].execute(c)
            for m in machines[1:]:assert m.execute(c)==expected,(m.label,expected)
        except Exception:
            print('Failing context case',i,{k:v for k,v in c.items() if k!='setup'},flush=True);raise
        counts[SLOTS[c['function']][0]]+=1
    summary=dict(installed=args.installed,functions=len(SLOTS),scenarios=len(cases),executions=3*len(cases),
        slots=sizes,scenarios_by_function=counts,seconds=round(time.monotonic()-started,3),
        checks=['all touched pages','caller arguments','callee-saved registers','six x87 control words and preserved control',
            'independent geometry','output aliases','wrapped count/coordinates'],
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in
            ('kernel/font_context.c','kernel/font_context.h','tests/kernel_font_context.py')})
    (BUILD/'font_context_summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))

if __name__=='__main__':main()
