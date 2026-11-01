"""Baked/packed atlas pixels and metadata through the real font pipeline."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct
import sys
import random
import time
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(BUILD/'test-deps')]
from tools.kernel_image import Fat16,ORIGINAL_SHA256
from tests.kernel_font_outline import compile_variants,fixture,words
from tests.kernel_font_heap import Machine as HeapMachine
from tests.kernel_font_flatten import heap_setup,CONTROLS
from tests.kernel_font_context import fbits
from tests.kernel_font_init import sfnt
from tests.kernel_font_shape import simple
from tests.kernel_font_glyph import real_info
from tests.kernel_font_atlas import rect_bytes,pack_oracle
from tests.kernel_font_cmap import STACK,STOP,START,CONTROL,MASK,SAVED,SENTINELS
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EFLAGS,UC_X86_REG_EIP,UC_X86_REG_FPCW
BASE,DATA,BUFFER,OUTPUT,HEAP,FONT=0x800000,0x600000,0x500000,0x510000,0x2000000,0xa00000
PIXELS,CHARS,RANGES,RECTS,CPS=OUTPUT+0x1000,OUTPUT+0x60000,DATA+0x80000,OUTPUT+0x70000,DATA+0x90000
SLOTS=[('font_bake','kernel_font_bake_832a98',0x832a98,0x832dbe),
 ('font_prefilter','kernel_font_prefilter_833bbc',0x833bbc,0x833c94),
 ('font_render_rects','kernel_font_render_rects_833c94',0x833c94,0x8342c4),
 ('font_pack_ranges','kernel_font_pack_ranges_8342e8',0x8342e8,0x8345c2),
 ('font_pack_range','kernel_font_pack_range_8345c2',0x8345c2,0x834612),
 ('font_scaled_metrics','kernel_font_scaled_metrics_834612',0x834612,0x8346e2),
 ('font_bake_api','kernel_font_bake_api_836f70',0x836f70,0x836fbc),
 ('font_offset_api','kernel_font_offset_api_836fbc',0x836fbc,0x836fd6),
 ('font_count_api','kernel_font_count_api_836fd6',0x836fd6,0x836fea),
 ('font_init_api','kernel_font_init_api_836fea',0x836fea,0x83700c)]
class Machine(HeapMachine):
    returns={0,2,3,4,6,7,8,9}
    def checkpoint(self,case):
        # Stop BEFORE either broken original filter executes; compare the real
        # caller's completed pixel rendering, metadata and filter arguments.
        uc=self.uc;stack_base=STACK+case.get('stack_offset',0)
        for page in self.old_pages:uc.mem_write(page,bytes(4096))
        self.pages=set();boundary=[]
        self.put(OUTPUT,bytes([0xa5])*256)
        for address,data in case['setup']:self.put(address,data)
        args=words(*case['args']);uc.mem_write(STACK-0x4000,bytes(0x5000));uc.mem_write(stack_base,words(STOP)+args)
        uc.mem_write(CONTROL,struct.pack('<H',case.get('control',0x37f)))
        entry=self.entries[case['function']]
        uc.mem_write(START,b'\xdb\xe3\xd9\x2d'+words(CONTROL)+b'\xe9'+words(entry-START-13))
        uc.ctl_remove_cache(START,START+16)
        uc.reg_write(UC_X86_REG_ESP,stack_base);uc.reg_write(UC_X86_REG_EFLAGS,2)
        for reg,value in zip(SAVED,SENTINELS):uc.reg_write(reg,value)
        def stop(machine,address,size,unused):
            if address in (0x833292,0x83357c):
                sp=machine.reg_read(UC_X86_REG_ESP)
                boundary.extend([address,bytes(machine.mem_read(sp+4,20))]);machine.emu_stop()
        hook=uc.hook_add(UC_HOOK_CODE,stop,begin=0x833292,end=0x83357c)
        try:uc.emu_start(START,0,count=2000000)
        finally:uc.hook_del(hook)
        assert boundary,(self.label,hex(uc.reg_read(UC_X86_REG_EIP)),case)
        assert uc.reg_read(UC_X86_REG_FPCW)==case.get('control',0x37f)
        caller=bytes(uc.mem_read(stack_base+4,len(args)))
        pages=[(page,hashlib.sha256(uc.mem_read(page,4096)).hexdigest()) for page in sorted(self.pages)]
        self.old_pages=self.pages
        return boundary,caller,pages


def font_fixture(offset=0):
    head=bytearray(64);struct.pack_into('>H',head,18,16);struct.pack_into('>h',head,50,1)
    hhea=bytearray(40);struct.pack_into('>hhh',hhea,4,12,-4,2);struct.pack_into('>H',hhea,34,3)
    glyph=simple([[(1,0,True),(7,0,True),(7,4,True),(1,4,True)]],True)
    glyph=bytearray(glyph);struct.pack_into('>5h',glyph,0,1,1,0,7,4)
    glyph=bytes(glyph)+bytes(len(glyph)&1)
    cmap=bytearray(12+262);struct.pack_into('>HHHHI',cmap,0,0,1,3,1,12)
    struct.pack_into('>3H',cmap,12,0,262,0);cmap[18+65]=1;cmap[18+66]=2
    return sfnt({b'cmap':bytes(cmap),b'loca':struct.pack('>4I',0,len(glyph),len(glyph)*2,len(glyph)*2),
        b'head':bytes(head),b'glyf':glyph*2,b'hhea':bytes(hhea),
        b'hmtx':struct.pack('>6h',10,1,12,1,8,0),b'maxp':struct.pack('>IH',0x10000,3)},offset)[0]

def font_info(raw):
    info=bytearray(real_info(raw)[0]);struct.pack_into('<I',info,4,DATA);return bytes(info)

def base(raw,limit=HEAP+0x100000):
    return [*heap_setup(limit),(DATA,raw),(PIXELS,b'\xa5'*0x10000),(CHARS,b'\xa5'*0x1000)]

def bitmap_rows(pixels,start,width,height,stride):
    # Rectangle has full coverage. Original writes 4*width scanline bytes,
    # preserving only width visible white pixels and zeroing all padding.
    for row in range(height):pixels[start+row*stride:start+row*stride+4*width]=bytes([255])*width+bytes(width*3)

def bake_oracle(width,rows,first,count,size):
    # Independent cell/row layout with the original >= boundary and 1px gap.
    pixels=bytearray(bytes(width*rows*4)+b'\xa5'*64);chars=bytearray(b'\xa5'*max(20,count*20))
    x=y=bottom=1;result=bottom
    for i in range(max(0,count)):
        empty=first+i==66;w,h=(0,0) if empty else (6,4)
        if x+w+1>=width:x=1;y=bottom
        if y+h+1>=rows:result=(-i)&0xffffffff;break
        if not empty:bitmap_rows(pixels,x+y*width,w,h,width)
        struct.pack_into('<4H3f',chars,i*20,x,y,x+w,y+h,0 if empty else 1,0 if empty else -4,8 if empty else 12 if first+i==65 else 10)
        x+=w+1;bottom=max(bottom,y+h+1);result=bottom
    return result,bytes(pixels),bytes(chars)

def bake_cases(raw):
    cases=[]
    for fn in (0,6):
        for width in (0,1,7,8,9,15,16,32):
            for rows in (0,1,5,6,7,12):
                for first,count in ((65,0),(65,1),(64,3),(65,4),(66,2)):
                    c=fixture(fn,[DATA,0,fbits(16),PIXELS,width,rows,first,count,CHARS],base(raw))
                    result,pixels,chars=bake_oracle(width,rows,first,count,16)
                    c.update(expected_return=result,expected_memory=[(PIXELS,pixels),(CHARS,chars)])
                    cases.append(c)
        for control in CONTROLS:
            for size in (0.,-0.,8,9.5,16,24):
                for limit in (HEAP,HEAP+1024,HEAP+2048,HEAP+4096,HEAP+0x100000):
                    cases.append(fixture(fn,[DATA,0,fbits(size),PIXELS,32,32,64,3,CHARS],base(raw,limit),control=control))
        for count in (-2147483648,-1,0):cases.append(fixture(fn,[DATA,0,fbits(16),PIXELS,8,4,65,count,CHARS],base(raw),1))
        for delta in (0,2,4,8):
            cases.append(fixture(fn,[DATA,0,fbits(16),PIXELS,32,16,65,2,PIXELS+delta],base(raw)))
        invalid=bytearray(raw);invalid[12:16]=b'xxxx'
        cases.append(fixture(fn,[DATA,0,fbits(16),PIXELS,16,12,65,1,CHARS],base(bytes(invalid)),-1))
    return cases

def ranges_bytes(groups,size=16,oversample=0x101):
    return b''.join(words(fbits(size),first,ptr,count,CHARS+i*0x200,oversample) for i,(first,ptr,count) in enumerate(groups))

def packed_case(fn,raw,width=32,height=16,size=16,first=64,count=4,skip=0,limit=HEAP+0x100000,control=0x37f):
    context=words(77,BUFFER+128,width,height,width,1,skip,1,1,PIXELS,0)
    setup=[*base(raw,limit),(BUFFER,context),(BUFFER+128,words(width-1,height-1,0,0,0)),
        (RANGES,ranges_bytes([(first,0,count)],size)),(CPS,words(65,0,9999,66,0))]
    args=[BUFFER,DATA,0,RANGES,1] if fn==3 else [BUFFER,DATA,0,fbits(size),first,count,CHARS]
    return fixture(fn,args,setup,control=control)

def render_cases(raw):
    cases=[];info=font_info(raw)
    for groups in ([(64,0,4)],[(0,CPS,5)],[(0,CPS,3),(0,CPS,5)]):
        for skip in (0,1):
            for pad in (0,1,3):
                for all_packed in (0,1):
                    for control in CONTROLS:
                        total=sum(g[2] for g in groups);dims=[(7,5) if n%3==0 else (0,0) for n in range(total)]
                        rects=bytearray(rect_bytes(dims))
                        for i in range(total):struct.pack_into('<3I',rects,i*24,1+i*8,1,0);struct.pack_into('<I',rects,i*24+20,all_packed)
                        cases.append(fixture(2,[BUFFER,BUFFER+256,RANGES,len(groups),RECTS],
                            [*base(raw),(BUFFER,words(77,0,128,32,128,pad,skip,3,4,PIXELS,0)),
                                (BUFFER+256,info),(RANGES,ranges_bytes(groups)),(RECTS,bytes(rects)),
                                (CPS,words(65,0,9999,66,0))],control=control))
    # Nonempty success with independent glyph metadata and complete scan rows.
    raw_rect=words(2,3,99,7,5,1)
    ch=struct.pack('<4H5f',3,4,9,8,1,-4,12,7,0)
    pixels=bytearray(b'\xa5'*0x10000);bitmap_rows(pixels,3+4*32,6,4,32)
    c=fixture(2,[BUFFER,BUFFER+256,RANGES,1,RECTS],[*base(raw),
        (BUFFER,words(77,0,32,16,32,1,0,3,4,PIXELS,0)),(BUFFER+256,info),
        (RANGES,ranges_bytes([(65,0,1)])),(RECTS,raw_rect)],1,
        [(CHARS,ch),(RECTS,words(3,4,99,6,4,1)),(PIXELS,bytes(pixels)),(BUFFER+28,words(3,4))]);cases.append(c)
    for count in (-1,0):cases.append(fixture(2,[BUFFER,BUFFER+256,RANGES,count,RECTS],[(BUFFER,words(*range(11)))],1))
    for h,v in ((0,0),(0,1),(1,0)):
        # All rectangles omitted: tests zero reciprocal without invoking filters.
        c=fixture(2,[BUFFER,BUFFER+256,RANGES,1,RECTS],[*base(raw),(BUFFER,words(77,0,32,16,32,1,0,3,4,PIXELS,0)),
            (BUFFER+256,info),(RANGES,ranges_bytes([(65,0,2)],oversample=h+v*256)),(RECTS,words(*([0]*12)))])
        cases.append(c)
    return cases

def pack_cases(raw):
    cases=[]
    for fn in (3,4):
        for size in (0,8,-8,16,-16,24):
            for width,height in ((8,6),(16,12),(32,16)):
                for skip in (0,1):
                    for limit in (HEAP,HEAP+1024,HEAP+2048,HEAP+4096,HEAP+0x100000):
                        cases.append(packed_case(fn,raw,width,height,size,skip=skip,limit=limit))
        for count in (0,1,2,4):
            for control in CONTROLS:cases.append(packed_case(fn,raw,count=count,control=control))
    for skip in (0,1):
        c=packed_case(3,raw,skip=skip)
        c['setup'][-2]=(RANGES,ranges_bytes([(0,CPS,5),(0,CPS,3)]));c['args'][-1]=2;cases.append(c)
    return cases

def prefilter_cases(raw):
    info=font_info(raw);cases=[]
    for width,height in ((0,0),(8,4),(16,8)):
        for dx,dy in ((0,0),(.25,.5),(-.5,-.25)):
            for control in CONTROLS:
                for glyph in (0,1,2):
                    cases.append(fixture(1,[BUFFER,PIXELS,width,height,64,fbits(1),fbits(1),fbits(dx),fbits(dy),1,1,OUTPUT,OUTPUT+4,glyph],
                        [*base(raw),(BUFFER,info)],memory=[(OUTPUT,words(0,0))],control=control))
    # Output alias changes the vertical oversample after horizontal shift store.
    c=fixture(1,[BUFFER,PIXELS,0,0,64,0,0,0,0,1,1,STACK+44,OUTPUT,2],[*base(raw),(BUFFER,info)])
    c.update(caller_alias=True,stack_offset=0);cases.append(c)
    return cases

def extra_cases(raw):
    cases=[]
    for h,v in ((0,1),(1,0),(2,3),(4,4),(255,255)):
        # No filter calls when records are unfilled; restoration still occurs.
        c=fixture(2,[BUFFER,BUFFER+256,RANGES,1,RECTS],[*base(raw),
            (BUFFER,words(77,0,32,16,32,1,0,3,4,PIXELS,0)),(BUFFER+256,font_info(raw)),
            (RANGES,ranges_bytes([(65,0,2)],oversample=h+v*256)),(RECTS,words(*([0]*12)))])
        cases.append(c)
    for slot in (0,1,2,3,4):
        c=fixture(5,[DATA,0,fbits(16),OUTPUT,OUTPUT+4,OUTPUT+8],[(DATA,raw)])
        c['args'][3]=STACK+4+slot*4
        if slot==4:c['args'][2]=fbits(-0.)
        c.update(caller_alias=True,stack_offset=0);cases.append(c)
    # Output char fields overlap range metadata: original captures four initial
    # rectangle targets before storing, then rereads counts and pointers.
    for delta in (0,2,20,22):
        c=packed_case(3,raw,count=1,limit=HEAP)
        c['setup'][-2]=(RANGES,ranges_bytes([(65,0,1)]))
        encoded=bytearray(c['setup'][-2][1]);struct.pack_into('<I',encoded,16,RANGES+delta)
        c['setup'][-2]=(RANGES,bytes(encoded));cases.append(c)
    return cases

def checkpoint_cases(raw):
    cases=[]
    for h,v in ((2,1),(3,1),(4,1),(1,2),(1,3),(1,4)):
        for control in CONTROLS:
            c=fixture(1,[BUFFER,PIXELS,16,8,64,fbits(1),fbits(1),0,0,h,v,OUTPUT,OUTPUT+4,1],
                [*base(raw),(BUFFER,font_info(raw))],control=control);cases.append(c)
            c=fixture(2,[BUFFER,BUFFER+256,RANGES,1,RECTS],[*base(raw),
                (BUFFER,words(77,0,64,32,64,1,0,3,4,PIXELS,0)),(BUFFER+256,font_info(raw)),
                (RANGES,ranges_bytes([(65,0,1)],oversample=h+v*256)),(RECTS,words(2,2,0,12,12,1))],control=control);cases.append(c)
    return cases

def api_cases(raw,font):
    cases=[]
    for fn,args in ((7,[DATA,0]),(8,[DATA]),(9,[BUFFER,DATA,0])):
        cases.append(fixture(fn,args,[(DATA,raw),(BUFFER,b'\xa5'*160)],1 if fn!=7 else 0))
    for index in (-1,0,1,2,3,0x7fffffff):
        collection=bytearray(raw);collection[:24]=b'ttcf'+struct.pack('>II3I',0x10000,3,256,512,1024)
        cases.append(fixture(7,[DATA,index],[(DATA,bytes(collection))],(256,512,1024)[index] if 0<=index<3 else 3 if index==-1 else -1))
    for signature in (b'xxxx',b'\0\x01\0\0',b'OTTO',b'true',b'typ1'):
        altered=bytearray(raw);altered[:4]=signature;cases.append(fixture(8,[DATA],[(DATA,bytes(altered))]))
    for size in (0,-0.,8,-8,16,-16,9.5,float('inf'),float('nan')):
        for control in CONTROLS:
            c=fixture(5,[DATA,0,fbits(size),OUTPUT,OUTPUT+4,OUTPUT+8],[(DATA,raw)],control=control)
            if size in (16,-16) and control==0x37f:c['expected_memory']=[(OUTPUT,struct.pack('<3f',12,-4,2))]
            cases.append(c)
    for delta in (0,1,4,8):cases.append(fixture(5,[DATA,0,fbits(16),OUTPUT+delta,OUTPUT,OUTPUT+2],[(DATA,raw)]))
    for data in (raw,font):
        # Real font's ASCII glyphs remain bounded; all actual nested helpers run.
        for first in (32,48,64,80):
            for fn in (0,3,4,5,6):
                if fn in (0,6):c=fixture(fn,[DATA,0,fbits(9),PIXELS,64,32,first,3,CHARS],base(data))
                elif fn in (3,4):c=packed_case(fn,data,64,32,9,first,3)
                else:c=fixture(5,[DATA,0,fbits(9),OUTPUT,OUTPUT+4,OUTPUT+8],[(DATA,data)])
                cases.append(c)
    return cases

def main():
    p=argparse.ArgumentParser(__doc__);p.add_argument('--installed',action='store_true');args=p.parse_args()
    start=time.monotonic();disk=Fat16((ROOT.parent/'IceStarOS_x86_32M.vhd').read_bytes());original=disk.read_kernel()
    assert hashlib.sha256(original).hexdigest()==ORIGINAL_SHA256
    variants,sizes=compile_variants(original,args.installed,'bake',SLOTS)
    machines=[Machine(*spec,original,b'') for spec in [('original',original,BASE,[s for _,_,s,_ in SLOTS]),*variants]]
    raw=font_fixture();font=disk.read_file(b'ICESTAR    ',b'FONT       ',b'FIRA    TTF')
    cases=bake_cases(raw)+render_cases(raw)+pack_cases(raw)+prefilter_cases(raw)+api_cases(raw,font)+extra_cases(raw)
    counts={n:0 for n,_,_,_ in SLOTS}
    for i,c in enumerate(cases):
        c.setdefault('stack_offset',4 if i&1 else 0)
        try:
            expected=machines[0].execute(c)
            for m in machines[1:]:
                actual=m.execute(c);assert actual==expected,(m.label,expected,actual)
        except Exception:
            print('Failing bake case',i,{k:v for k,v in c.items() if k not in ('setup','expected_memory')},flush=True);raise
        counts[SLOTS[c['function']][0]]+=1
        if (i+1)%500==0:print(f'Compared {i+1}/{len(cases)} bake scenarios',flush=True)
    checkpoints=checkpoint_cases(raw)
    for i,c in enumerate(checkpoints):
        c['stack_offset']=4 if i&1 else 0
        try:
            expected=machines[0].checkpoint(c)
            for m in machines[1:]:assert m.checkpoint(c)==expected,(m.label,expected)
        except Exception:
            print('Failing filter-entry checkpoint',i,{k:v for k,v in c.items() if k!='setup'},flush=True);raise
    summary=dict(filter_entry_checkpoints=len(checkpoints),checkpoint_executions=len(checkpoints)*3,installed=args.installed,functions=len(SLOTS),scenarios=len(cases),executions=len(cases)*3,
        slots=sizes,scenarios_by_function=counts,seconds=round(time.monotonic()-start,3),
        checks=['real SFNT init, cmap, metrics, shape, heap, flatten, sort and raster calls; no stubs',
            'independent baking layout, packed metadata and every pixel including DWORD padding',
            'all touched data/heap pages, caller words, stack balance, callee saves and x87 control',
            'six x87 controls, two stack alignments, boundaries, zero glyphs, missing glyph across ranges',
            'allocation failures, rectangle failure flags, restored oversamples, overlapping outputs',
            'sampled real Fira ASCII pipelines','real oversampling 2/3/4 paths stopped at actual broken filter entry, full pre-entry memory and arguments compared'],
        limitations='Original filters remain: successful-return coverage uses oversampling 1 (or empty rectangles). Broken filter paths are not claimed equivalent after entry.',
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ('kernel/font_bake.c','kernel/font_bake.h','tests/kernel_font_bake.py')})
    (BUILD/'font_bake_summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
if __name__=='__main__':main()
