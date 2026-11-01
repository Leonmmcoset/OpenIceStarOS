"""TrueType simple/compound and CFF shapes against original x86 and geometry."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import sys
import time
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(BUILD/'test-deps')]
from tools.kernel_image import Fat16,ORIGINAL_SHA256
from tests.kernel_font_outline import Machine as BaseMachine,compile_variants,fixture,words
from tests.kernel_font_cmap import STACK,STOP,START,CONTROL,MASK,SAVED,SENTINELS
from tests.kernel_font_glyph import real_info,font_info
from tests.kernel_font_cmap import independent_map
from tests.kernel_font_parse import index_data
from unicorn import UC_HOOK_CODE,UcError
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EFLAGS,UC_X86_REG_EIP,UC_X86_REG_FPCW
BASE,DATA,BUFFER,OUTPUT,FONT,HEAP=0x800000,0x600000,0x500000,0x510000,0xa00000,0x2000000
SLOTS=[('font_truetype_shape','kernel_font_truetype_shape_82d386',0x82d386,0x82e05c),
 ('font_cff_shape','kernel_font_cff_shape_82f756',0x82f756,0x82f80a),
 ('font_shape','kernel_font_shape_82f90a',0x82f90a,0x82f958),
 ('font_codepoint_shape','kernel_font_codepoint_shape_82cede',0x82cede,0x82cf10)]

class Machine(BaseMachine):
    aggregates=set();returns={0,1,2,3};mutable_caller=set()
    def __init__(self,*args):
        super().__init__(*args);self.uc.mem_map(HEAP,0x400000)
        self.uc.hook_add(UC_HOOK_CODE,self.allocation,begin=0x817590,end=0x817590)

    def allocation(self,uc,address,size,unused):
        # Observe a provably nonterminating original heap traversal, without
        # replacing helpers or repairing memory. REP MOVSD in compound joins
        # may have overwritten a header with vertex/source bytes. Stop at the
        # shared malloc entry only if no first-fit block precedes the cycle.
        request=struct.unpack('<I',uc.mem_read(uc.reg_read(UC_X86_REG_ESP)+4,4))[0]
        request=(request+0x40f)&0xfffffc00;node=HEAP;seen=set()
        while HEAP<=node<=HEAP+0x400000-16:
            if node in seen:
                self.terminal=('cyclic_heap',node);uc.emu_stop();return
            seen.add(node)
            following,previous,used,owner=struct.unpack('<4I',uc.mem_read(node,16))
            if not following or (not used and (following-node)&MASK>=request):return
            node=following

    def execute(self,case):
        uc=self.uc;stack_base=STACK+case.get('stack_offset',0)
        for page in self.old_pages:uc.mem_write(page,bytes(4096))
        self.pages,self.done,self.fault,self.terminal=set(),False,None,None
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
        try:uc.emu_start(START,0,count=2000000)
        except UcError:
            assert self.fault is not None
            self.terminal=('fault',self.fault)
        assert self.done or self.terminal,(self.label,case,hex(uc.reg_read(UC_X86_REG_EIP)))
        caller=bytes(uc.mem_read(stack_base+4,len(arguments)*4))
        if not case.get('caller_alias'):assert caller==stack[4:],'caller arguments changed'
        value=None
        if self.done:
            assert uc.reg_read(UC_X86_REG_ESP)==stack_base+4,'stack balance'
            assert tuple(uc.reg_read(reg) for reg in SAVED)==SENTINELS,'callee-saved registers'
            assert uc.reg_read(UC_X86_REG_FPCW)==case.get('control',0x37f),'x87 control changed'
            value=uc.reg_read(UC_X86_REG_EAX)
        if 'expected_return' in case:assert self.done and value==case['expected_return']&MASK
        if 'vertices' in case and self.done:
            pointer=struct.unpack('<I',uc.mem_read(case['args'][2],4))[0]
            vertices=case['vertices'];assert value==len(vertices),(self.label,'vertex count',value,len(vertices))
            if vertices:
                actual=bytes(uc.mem_read(pointer,len(vertices)*14))
                for i,v in enumerate(vertices):
                    x,y,cx,cy,_,_,kind=struct.unpack_from('<6hB',actual,i*14)
                    expected=(kind,x,y,cx,cy) if kind in (3,4) else (kind,x,y)
                    assert expected==v,(self.label,i,expected,v)
        memory=[(page,hashlib.sha256(uc.mem_read(page,4096)).hexdigest()) for page in sorted(self.pages)]
        self.old_pages=self.pages
        return value,caller,memory,self.terminal


def setup_heap(limit=HEAP+0x400000,pattern=None):
    setup=[(HEAP,words(0,0,0,0)),(0x1b050,words(limit)),(0x1b0d8,words(23))]
    if pattern is not None:setup.insert(0,(HEAP,bytes([pattern])*0x20000))
    return setup


def simple(contours,compressed=False,instructions=b''):
    points=[p for contour in contours for p in contour]
    ends=[];total=0
    for contour in contours:total+=len(contour);ends.append(total-1)
    raw=struct.pack('>5h',len(contours),0,0,0,0)+struct.pack('>'+'H'*len(ends),*ends)+struct.pack('>H',len(instructions))+instructions
    flags=[];streams=[bytearray(),bytearray()];last=[0,0]
    for x,y,on in points:
        flag=int(on)
        for axis,value in enumerate((x,y)):
            delta=(value-last[axis]+32768)%65536-32768;last[axis]=value
            if compressed and delta==0:flag|=16<<axis
            elif compressed and abs(delta)<=255:
                flag|=2<<axis
                if delta>=0:flag|=16<<axis
                streams[axis].append(abs(delta))
            else:streams[axis]+=struct.pack('>h',delta)
        flags.append(flag)
    encoded=bytearray();i=0
    while i<len(flags):
        end=i+1
        while end<len(flags) and flags[end]==flags[i] and end-i<256:end+=1
        if compressed and end-i>1:encoded+=bytes([flags[i]|8,end-i-1])
        else:encoded+=bytes(flags[i:end])
        i=end
    return raw+encoded+b''.join(streams)


def compound(components):
    raw=bytearray(struct.pack('>5h',-1,0,0,0,0))
    for i,(flags,glyph,args,scale) in enumerate(components):
        flags=flags|32 if i+1<len(components) else flags&~32
        raw+=struct.pack('>HH',flags,glyph)
        if flags&2:raw+=struct.pack('>hh' if flags&1 else '>bb',*args)
        raw+=struct.pack('>'+'h'*len(scale),*scale)
    return bytes(raw)


def font_fixture(glyphs,fmt=1):
    raw=bytearray(0x4000);offsets=[0]
    for glyph in glyphs:
        raw[1024+offsets[-1]:1024+offsets[-1]+len(glyph)]=glyph
        offsets.append(offsets[-1]+len(glyph)+(len(glyph)&1))
    for i,offset in enumerate(offsets):
        struct.pack_into('>H' if fmt==0 else '>I',raw,128+i*(2 if fmt==0 else 4),offset//2 if fmt==0 else offset)
    struct.pack_into('>HHH',raw,512,0,262,0)
    for i in range(256):raw[518+i]=i if i<len(glyphs) else 0
    return font_info(glyphs=len(glyphs),loca=128,glyf=1024,loca_format=fmt,cmap=512),bytes(raw)


def geometry(contours):
    # Work from each closed contour's implied on-curve ring rather than the
    # executable's in-place scratch buffer or its contour state machine.
    vertices=[]
    for points in contours:
        first=points[0]
        if first[2]:start=first;ring=points[1:]+[first]
        elif points[1][2]:start=points[1];ring=points[2:]+[first,points[1]]
        else:
            start=((first[0]+points[1][0])>>1,(first[1]+points[1][1])>>1,True)
            ring=points[1:]+[first,start]
        vertices.append((1,start[0],start[1]));j=0
        while j<len(ring):
            point=ring[j]
            if point[2]:vertices.append((2,point[0],point[1]));j+=1
            else:
                next=ring[j+1]
                endpoint=next if next[2] else ((point[0]+next[0])>>1,(point[1]+next[1])>>1,True)
                vertices.append((3,endpoint[0],endpoint[1],point[0],point[1]))
                j+=2 if next[2] else 1
    return vertices


def synthetic_cases():
    cases=[];rng=random.Random(0x82d386)
    fixed=[[(0,0,True),(100,0,True),(100,100,True),(0,100,True)],
        [(0,0,True),(100,0,False),(100,100,True),(0,100,False)],
        [(-51,-31,False),(100,0,False),(100,100,True),(0,100,False)],
        [(-32768,32767,False),(32767,-32768,True),(0,0,True)]]
    contour_sets=[[contour] for contour in fixed]+[[fixed[0],fixed[1]],[]]
    for i in range(64):
        contours=[]
        for j in range(1+i%3):
            contours.append([(rng.randrange(-2048,2048),rng.randrange(-2048,2048),rng.choice((0,1))) for k in range(3+i%12)])
        contour_sets.append(contours)
    for contours in contour_sets:
        for compressed in (False,True):
            for fmt in (0,1):
                info,raw=font_fixture([simple(contours,compressed,b'\xb0\0')],fmt)
                for fn in (0,2,3):
                    c=fixture(fn,[BUFFER,0,OUTPUT],[(BUFFER,info),(DATA,raw),*setup_heap()])
                    c['vertices']=geometry(contours) if contours else [];cases.append(c)
    base=simple([fixed[0]],True)
    transforms=[(3,0,(31,-27),()),(2,0,(-5,7),()),(0,0,(),()),
        (11,0,(20,-10),(8192,)),(67,0,(20,-10),(8192,24576)),
        (131,0,(30,-20),(16384,4096,-8192,16384)),
        (11,0,(32767,-32768),(32767,)),(67,0,(0,0),(-16384,0)),
        (131,0,(-127,127),(-32768,32767,32767,-32768))]
    for transform in transforms:
        for count in (1,2,3):
            components=[transform]*count
            glyph=compound(components)
            info,raw=font_fixture([base,glyph,compound([(3,1,(10,20),())])])
            for index in (1,2):
                for control in (0x37f,0x7f,0x27f,0x77f,0xb7f,0xf7f):
                    cases.append(fixture(2,[BUFFER,index,OUTPUT],[(BUFFER,info),(DATA,raw),*setup_heap()],control=control))
    # Empty/missing outlines and allocation failures at successive recursive
    # and join allocations; preserve the real heap's partial effects.
    info,raw=font_fixture([base,compound([transforms[0],transforms[1],transforms[3]]),b''])
    for glyph in (0,1,2,3,-1):
        if glyph==-1:continue
        for limit in range(0,9216,1024):
            cases.append(fixture(2,[BUFFER,glyph,OUTPUT],[(BUFFER,info),(DATA,raw),*setup_heap(HEAP+limit)]))
    for output in (BUFFER+4,DATA+1024,HEAP+16,HEAP+1024):
        cases.append(fixture(0,[BUFFER,0,output],[(BUFFER,info),(DATA,raw),*setup_heap()]))
    return cases


def cff_cases():
    programs=[b'\x0e',bytes([139,139,21,239,139,5,139,239,5,39,139,5,14]),
        bytes([139,139,21,149,159,169,179,189,199,8,14]),b'\x00',b'']
    expected=[[],[(1,0,0),(2,100,0),(2,100,100),(2,0,100),(2,0,0)],
        [(1,0,0),(4,90,120,10,20),(2,0,0)],[],[]]
    cases=[]
    for program,vertices in zip(programs,expected):
        index=index_data([program]);info=bytearray(font_info(cff_size=1,cmap=512));struct.pack_into('<3I',info,68,DATA,0,len(index))
        cmap=struct.pack('>HHH',0,262,0)+bytes(256)
        for fn in (1,2,3):
            c=fixture(fn,[BUFFER,65 if fn==3 else 0,OUTPUT],[(BUFFER,bytes(info)),(DATA,index),(DATA+512,cmap),*setup_heap()]);c['vertices']=vertices;cases.append(c)
    return cases


def main():
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--installed',action='store_true');parser.add_argument('--synthetic-only',action='store_true');args=parser.parse_args()
    started=time.monotonic();disk=Fat16((ROOT.parent/'IceStarOS_x86_32M.vhd').read_bytes());original=disk.read_kernel();assert hashlib.sha256(original).hexdigest()==ORIGINAL_SHA256
    font=disk.read_file(b'ICESTAR    ',b'FONT       ',b'FIRA    TTF');variants,sizes=compile_variants(original,args.installed,'shape',SLOTS)
    machines=[Machine(*spec,original,font) for spec in [('original',original,BASE,[s for _,_,s,_ in SLOTS]),*variants]]
    cases=synthetic_cases()+cff_cases();real_counts={'simple':0,'compound':0,'empty':0}
    if not args.synthetic_only:
        info,tables,count,fmt,selected=real_info(font);mapping=independent_map(font,selected)
        for glyph in range(count):
            c=fixture(2,[BUFFER,glyph,OUTPUT],[(BUFFER,info),*setup_heap()]);cases.append(c)
            def location(g):return struct.unpack_from('>H' if fmt==0 else '>I',font,tables[b'loca']+g*(2 if fmt==0 else 4))[0]*(2 if fmt==0 else 1)
            key='empty' if location(glyph)==location(glyph+1) else 'compound' if struct.unpack_from('>h',font,tables[b'glyf']+location(glyph))[0]<0 else 'simple'
            real_counts[key]+=1
        for cp in list(range(128))+[256,1024,65535,65536,0x10ffff]:
            cases.append(fixture(3,[BUFFER,cp,OUTPUT],[(BUFFER,info),*setup_heap()]))
    counts={n:0 for n,_,_,_ in SLOTS};terminals={};failures=[]
    for i,c in enumerate(cases):
        c['stack_offset']=4 if i&1 else 0
        try:
            expected=machines[0].execute(c)
            for m in machines[1:]:
                actual=m.execute(c);assert actual==expected,(m.label,expected,actual)
        except Exception:
            print('Failing shape case',i,{k:v for k,v in c.items() if k not in ('setup','vertices')},flush=True);raise
        counts[SLOTS[c['function']][0]]+=1
        terminal='returned' if expected[3] is None else expected[3][0]
        terminals[terminal]=terminals.get(terminal,0)+1
        if expected[3] is not None:
            failures.append(dict(case=i,function=SLOTS[c['function']][0],glyph_or_codepoint=c['args'][1],terminal=expected[3]))
        if (i+1)%1000==0:print(f'Compared {i+1}/{len(cases)} shape scenarios',flush=True)
    summary=dict(installed=args.installed,functions=len(SLOTS),scenarios=len(cases),executions=len(cases)*3,real_fira=real_counts,slots=sizes,
        terminal_counts=terminals,original_failure_cases=failures,synthetic_only=args.synthetic_only,
        scenarios_by_function=counts,seconds=round(time.monotonic()-started,3),
        checks=['real nested calls and heap allocator','all touched pages including allocation slack and headers',
            *(['all shipped Fira simple/compound/empty glyphs, including original failures'] if not args.synthetic_only else []),
            'independent synthetic contour geometry','compressed/repeated flags',
            'all component transform forms','six x87 controls','allocation failures','output/source/heap aliases','CFF two-pass output'],
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ('kernel/font_shape.c','kernel/font_shape.h','tests/kernel_font_shape.py')})
    (BUILD/'font_shape_summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))

if __name__=='__main__':main()
