"""Real x86/ordinary/installed atlas dimensions, packing and drawing quads."""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import random
import struct
import sys
import time
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(BUILD/'test-deps')]
from tools.kernel_image import Fat16,ORIGINAL_SHA256
from tests.kernel_font_outline import compile_variants,fixture,words,signed
from tests.kernel_font_heap import Machine as HeapMachine
from tests.kernel_font_flatten import heap_setup,CONTROLS
from tests.kernel_font_context import fbits
from tests.kernel_font_shape import simple,font_fixture
from tests.kernel_font_glyph import real_info
from tests.kernel_font_cmap import STACK
BASE,DATA,BUFFER,OUTPUT,HEAP,FONT=0x800000,0x600000,0x500000,0x510000,0x2000000,0xa00000
SLOTS=[('font_baked_quad','kernel_font_baked_quad_832dbe',0x832dbe,0x832f44),
 ('font_rect_init','kernel_font_rect_init_832f44',0x832f44,0x832f78),
 ('font_rect_pack','kernel_font_rect_pack_832f78',0x832f78,0x8330f6),
 ('font_pack_begin','kernel_font_pack_begin_8330f6',0x8330f6,0x83323c),
 ('font_pack_end','kernel_font_pack_end_83323c',0x83323c,0x833260),
 ('font_pack_oversample','kernel_font_pack_oversample_833260',0x833260,0x833284),
 ('font_pack_skip','kernel_font_pack_skip_833284',0x833284,0x833292),
 ('font_oversample_shift','kernel_font_oversample_shift_8338c8',0x8338c8,0x8338fa),
 ('font_gather_rects','kernel_font_gather_rects_8338fa',0x8338fa,0x833bbc),
 ('font_pack_rects','kernel_font_pack_rects_8342c4',0x8342c4,0x8342e8),
 ('font_packed_quad','kernel_font_packed_quad_8346e2',0x8346e2,0x83488c)]
class Machine(HeapMachine):
    floats={7};returns={3,8}

def rect_bytes(dimensions):return b''.join(words(0xa5a5a5a5,0xa5a5a5a5,i,w,h,0xa5a5a5a5) for i,(w,h) in enumerate(dimensions))

def pack_oracle(ctx,rects,count):
    # A row layout simulation independent of the C pointers: record coordinates,
    # row heights and the first vertical overflow; remaining flags become zero.
    ctx=list(ctx);result=bytearray(rects);stopped=False
    for i in range(max(0,count)):
        w,h=struct.unpack_from('<2I',result,i*24+12)
        if not stopped:
            if signed(ctx[2]+w)>signed(ctx[0]):ctx[2]=0;ctx[3]=ctx[4]
            if signed(ctx[3]+h)>signed(ctx[1]):stopped=True
        if stopped:struct.pack_into('<I',result,i*24+20,0)
        else:
            struct.pack_into('<2I',result,i*24,ctx[2],ctx[3]);struct.pack_into('<I',result,i*24+20,1)
            ctx[2]=(ctx[2]+w)&0xffffffff
            if signed(ctx[3]+h)>signed(ctx[4]):ctx[4]=(ctx[3]+h)&0xffffffff
    return words(*ctx),bytes(result)

def packing_cases():
    rng=random.Random(0x832f78);cases=[]
    for n in range(240):
        w,h=rng.randrange(0,32),rng.randrange(0,32)
        ctx=(w,h,0,0,0);dims=[(rng.randrange(0,40),rng.randrange(0,20)) for _ in range(n%19)]
        raw=rect_bytes(dims);ec,er=pack_oracle(ctx,raw,len(dims))
        for fn in (2,9):
            setup=[(BUFFER,words(*ctx)),(DATA,raw)]
            args=[BUFFER,DATA,len(dims)]
            if fn==9:setup.append((BUFFER+64,words(0,BUFFER)));args[0]=BUFFER+64
            cases.append(fixture(fn,args,setup,memory=[(BUFFER,ec),(DATA,er)]))
    boundaries=(0,1,0x7fffffff,0x80000000,0xffffffff)
    for value in boundaries:
        for x in boundaries:
            for y in boundaries:
                ctx=(value,value,x,y,x);raw=rect_bytes([(1,1),(0xffffffff,0),(0,0x80000000)])
                cases.append(fixture(2,[BUFFER,DATA,3],[(BUFFER,words(*ctx)),(DATA,raw)]))
    for count in (-2147483648,-1,0):cases.append(fixture(2,[BUFFER,DATA,count]))
    # Rect fields overwrite live context fields, including width/row end.
    for delta in range(0,48,4):
        cases.append(fixture(2,[DATA+delta,DATA,3],[(DATA,rect_bytes([(8,4),(6,5),(1,1)]))]))
    for w,h in [(0,0),(7,4),(-1,-2),(0x7fffffff,0x80000000)]:
        cases.append(fixture(1,[BUFFER,w,h,0,0],[(BUFFER,b'\xa5'*64)],memory=[(BUFFER,words(w,h,0,0,0))]))
    # Caller alias: first store changes height or context before next store.
    for slot in (0,1,2):
        c=fixture(1,[STACK+4+slot*4,BUFFER,4,0,0]);c['caller_alias']=True;cases.append(c)
    return cases

def context_cases():
    cases=[]
    for w,h in ((0,0),(1,1),(8,4),(16,8),(0,8)):
        for pad in (0,1,4):
            for stride in (0,7,32):
                for limit in (HEAP,HEAP+1024,HEAP+2048,HEAP+4096,HEAP+0x100000):
                    for pixels in (0,OUTPUT):
                        c=fixture(3,[BUFFER,pixels,w,h,stride,pad,77],[*heap_setup(limit),(BUFFER,b'\xa5'*64),(OUTPUT,b'\xa5'*4096)])
                        # Ordinary forward allocations: success iff both 1KiB
                        # chunks fit strictly below limit. Negative node sizes
                        # retain original wrapping and get parity only.
                        if w>=pad:
                            success=limit>HEAP+2048
                            c['expected_return']=int(success)
                            if success:
                                c['expected_memory']=[(BUFFER,words(77,HEAP+16,w,h,stride or w,pad,0,1,1,pixels,HEAP+1040)),
                                    (HEAP+16,words(w-pad,h-pad,0,0,0))]
                                if pixels:c['expected_memory'] += [(pixels,bytes(w*h*4)),(pixels+w*h*4,b'\xa5')]
                            else:c['expected_memory']=[(BUFFER,b'\xa5'*64)]
                        cases.append(c)
    for pixels in (BUFFER,BUFFER+4,HEAP+16,HEAP+1040):
        cases.append(fixture(3,[BUFFER,pixels,8,4,0,1,77],[*heap_setup(),(BUFFER,b'\xa5'*256)]))
    for context in (HEAP+16,HEAP+1040):
        cases.append(fixture(3,[context,OUTPUT,8,4,32,1,77],heap_setup()))
    for null in (0,1,2,3):
        setup=heap_setup()
        setup.extend([(HEAP,words(HEAP+1024,0,1,23)),(HEAP+1024,words(HEAP+2048,HEAP,1,23)),
            (HEAP+2048,words(0,HEAP+1024,0,0)),(BUFFER,words(77,HEAP+16 if null&1 else 0,*([0]*8),HEAP+1040 if null&2 else 0))])
        cases.append(fixture(4,[BUFFER],setup))
    for h in (0,1,2,3,4,5,256,0x80000000,0xffffffff):
        for v in (0,1,4,5,0xffffffff):
            raw=bytearray(b'\xa5'*44)
            if h<=4:struct.pack_into('<I',raw,28,h)
            if v<=4:struct.pack_into('<I',raw,32,v)
            cases.append(fixture(5,[BUFFER,h,v],[(BUFFER,b'\xa5'*44)],memory=[(BUFFER,bytes(raw))]))
    for skip in (0,1,2,0x80000000,0xffffffff):cases.append(fixture(6,[BUFFER,skip],memory=[(BUFFER+24,words(skip))]))
    # First oversample store changes vertical caller word; reload is required.
    for off in (0,4):
        c=fixture(5,[STACK+off+12-28,1,4]);c.update(caller_alias=True,stack_offset=off);cases.append(c)
    for factor in (0,1,2,3,4,5,16,127,255,65535,0x7fffffff,0x80000000,0xffffffff):
        for control in CONTROLS:
            c=fixture(7,[factor],control=control)
            if control==0x37f:
                expected=0 if not factor else signed(1-factor)/(2*signed(factor))
                c['expected_float']=struct.pack('<f',expected)
            cases.append(c)
    return cases

def quad_case(fn,xy=(10.25,-2.75),offsets=(1.25,-1.5,7.5,9.25,2.5),dims=(64,32),flags=0,control=0x37f,oracle=True):
    raw=struct.pack('<4H5f',2,3,8,9,*offsets)
    if fn==0:raw=raw[:20]
    c=fixture(fn,[DATA,*dims,0,BUFFER,BUFFER+4,OUTPUT,flags],[(DATA,raw),(BUFFER,struct.pack('<2f',*xy))],control=control)
    if oracle and control==0x37f and dims[0] and dims[1] and all(math.isfinite(x) for x in (*xy,*offsets)):
        x,y=xy;xo,yo,adv,xo2,yo2=offsets
        if fn==0:
            rx,ry=math.floor(x+xo+.5),math.floor(y+yo+.5);bias=0 if flags else -.5
            a,b,cc,d=rx+bias,ry+bias,rx+6+bias,ry+6+bias
        elif flags:
            rx,ry=math.floor(x+xo+.5),math.floor(y+yo+.5);a,b,cc,d=rx,ry,xo2+rx-xo,yo2+ry-yo
        else:a,b,cc,d=x+xo,y+yo,x+xo2,y+yo2
        # Texture multiplication starts from float32 reciprocals.
        iw,ih=(struct.unpack('<f',struct.pack('<f',1/n))[0] for n in dims)
        c['expected_memory']=[(OUTPUT,struct.pack('<8f',a,b,2*iw,3*ih,cc,d,8*iw,9*ih)),(BUFFER,struct.pack('<f',x+adv))]
    return c

def quad_cases():
    cases=[];rng=random.Random(0x8346e2)
    for fn in (0,10):
        for flag in (0,1,-1):
            for control in CONTROLS:
                for xy in ((10.25,-2.75),(-.5,-1.5),(0.,-0.),(16777217.,-16777217.)):
                    for dims in ((64,32),(17,31),(-3,-7),(0,0)):
                        cases.append(quad_case(fn,xy,dims=dims,flags=flag,control=control,oracle=abs(xy[0])<1000))
        for _ in range(120):
            cases.append(quad_case(fn,(rng.randrange(-1000,1000)/4,rng.randrange(-1000,1000)/4),
                tuple(rng.randrange(-64,64)/4 for _ in range(5)),flags=rng.randrange(2)))
        for delta in range(0,32,4):
            for flags in (0,1):
                c=quad_case(fn,flags=flags,oracle=False);c['args'][6]=DATA+delta;cases.append(c)
                c=quad_case(fn,flags=flags,oracle=False);c['args'][4]=OUTPUT+delta;cases.append(c)
                c=quad_case(fn,flags=flags,oracle=False);c['args'][5]=OUTPUT+delta;cases.append(c)
        for value in (math.inf,-math.inf,math.nan,-0.,1e-40,2147483648.):
            for flags in (0,1):cases.append(quad_case(fn,(value,value),flags=flags,oracle=False))
        for index in (-1,1,0x40000000,0x80000000):
            c=quad_case(fn,oracle=False);c['args'][0]=(DATA-index*(20 if fn==0 else 28))&0xffffffff;c['args'][3]=index;cases.append(c)
    return cases

def gather_case(info,raw,ranges,h=1,v=1,pad=1,skip=0,control=0x37f):
    encoded=b''.join(words(fbits(size),first,ptr,count,0,0xa5a5a5a5) for size,first,ptr,count in ranges)
    total=sum(max(0,entry[3]) for entry in ranges)
    return fixture(8,[BUFFER,BUFFER+256,DATA+0x8000,len(ranges),OUTPUT],
        [(BUFFER,words(77,0,64,32,64,pad,skip,h,v,0,0)),(BUFFER+256,info),(DATA,raw),
            (DATA+0x8000,encoded),(DATA+0x9000,words(65,0,9999,66,0)),(OUTPUT,b'\xa5'*(24*max(1,total)))],expected=total,control=control)

def gather_cases(font):
    glyph=bytearray(simple([[(-2,-1,True),(6,-1,True),(6,4,True),(-2,4,True)]],True));struct.pack_into('>5h',glyph,0,1,-2,-1,6,4)
    info,raw=font_fixture([bytes(glyph),bytes(glyph),b''])
    # Install synthetic head/hhea and a format-0 cmap. Units 16, line height 16.
    info=bytearray(info);raw=bytearray(raw);raw.extend(bytes(max(0,0x3000-len(raw))))
    struct.pack_into('<I',info,20,0x2000);struct.pack_into('<I',info,28,0x2100)
    struct.pack_into('<I',info,48,0x2200)
    struct.pack_into('>H',raw,0x2012,16);struct.pack_into('>hh',raw,0x2104,12,-4)
    struct.pack_into('>HHH',raw,0x2200,0,262,0);raw[0x2206+65]=1;raw[0x2206+66]=2
    cases=[]
    for size in (16,-16,8,-8,0,-0.,math.inf,math.nan):
        for h,v in ((1,1),(2,3),(4,4),(0,0),(0xffffffff,0x80000000)):
            for skip in (0,1):
                for control in CONTROLS:
                    c=gather_case(bytes(info),bytes(raw),[(size,0,DATA+0x9000,5),(size,0,DATA+0x9000,3)],h,v,skip=skip,control=control)
                    if abs(size)==16 and (h,v)==(1,1) and control==0x37f:
                        dims=[(9,6),(0,0) if skip else (9,6),(0,0),(1,1),(0,0),(9,6),(0,0),(0,0)]
                        c['expected_memory']=[(OUTPUT+i*24+12,words(*d)) for i,d in enumerate(dims)]
                    cases.append(c)
    for count in (-1,0,1,4):cases.append(gather_case(bytes(info),bytes(raw),[(16,64,0,count)]))
    for slot in (0,1,2,3,4):
        c=gather_case(bytes(info),bytes(raw),[(16,65,0,1)])
        # width store aliases a caller word only on the final rectangle.
        c['args'][4]=STACK+4+slot*4-12;c.update(caller_alias=True,stack_offset=0)
        if slot==3:c['setup'][3]=(DATA+0x8000,words(fbits(16),66,0,1,0,0xa5a5a5a5))
        cases.append(c)
    for addr in (BUFFER,BUFFER+4,BUFFER+12,BUFFER+16,BUFFER+20,BUFFER+28,DATA+0x8000):
        c=gather_case(bytes(info),bytes(raw),[(16,65,0,1)]);c['args'][4]=addr;c.pop('expected_return',None);cases.append(c)
    real,tables,count,fmt,_=real_info(font)
    for first in (0,32,64,96,128,256,512,1024,4096):
        for size in (16,-16,9.5):
            c=gather_case(real,b'',[(size,first,0,48)],2,3);c['setup'].append((FONT,font));cases.append(c)
    return cases

def main():
    p=argparse.ArgumentParser(__doc__);p.add_argument('--installed',action='store_true');args=p.parse_args()
    start=time.monotonic();disk=Fat16((ROOT.parent/'IceStarOS_x86_32M.vhd').read_bytes());original=disk.read_kernel()
    assert hashlib.sha256(original).hexdigest()==ORIGINAL_SHA256
    variants,sizes=compile_variants(original,args.installed,'atlas',SLOTS)
    machines=[Machine(*spec,original,b'') for spec in [('original',original,BASE,[s for _,_,s,_ in SLOTS]),*variants]]
    cases=packing_cases()+context_cases()+quad_cases()+gather_cases(disk.read_file(b'ICESTAR    ',b'FONT       ',b'FIRA    TTF'))
    counts={n:0 for n,_,_,_ in SLOTS}
    for i,c in enumerate(cases):
        c.setdefault('stack_offset',4 if i&1 else 0)
        try:
            expected=machines[0].execute(c)
            for m in machines[1:]:
                actual=m.execute(c);assert actual==expected,(m.label,expected,actual)
        except Exception:
            print('Failing atlas case',i,{k:v for k,v in c.items() if k not in ('setup','expected_memory')},flush=True);raise
        counts[SLOTS[c['function']][0]]+=1
        if (i+1)%1000==0:print(f'Compared {i+1}/{len(cases)} atlas scenarios',flush=True)
    summary=dict(installed=args.installed,functions=len(SLOTS),scenarios=len(cases),executions=len(cases)*3,
        slots=sizes,scenarios_by_function=counts,seconds=round(time.monotonic()-start,3),
        checks=['real malloc/free, floor, cmap, metrics and bounding boxes; no helper stubs',
            'independent row layout, dimensions, quads/texture coordinates, pixel DWORD clearing',
            'all touched heap/data pages, caller words, callee saves, stack balance, x87 control',
            'six x87 precision/rounding controls, both stack alignments, raw32/raw80 shift results',
            'zero/negative/wrapped dimensions; live aliases; missing glyph shared across ranges',
            'allocation failures at each stage; stale pack-end fields; original logo retained'],
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in
            ('kernel/font_atlas.c','kernel/font_atlas.h','tests/kernel_font_atlas.py','tests/kernel_font_outline.py')})
    (BUILD/'font_atlas_summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
if __name__=='__main__':main()
