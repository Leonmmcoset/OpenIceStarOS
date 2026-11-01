"""Real original/ordinary/installed outline edges and glyph bitmap pipeline."""
import argparse
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
from tests.kernel_font_outline import compile_variants,fixture,words
from tests.kernel_font_heap import Machine as HeapMachine
from tests.kernel_font_flatten import heap_setup,vertex,CONTROLS
from tests.kernel_font_context import fbits
from tests.kernel_font_shape import simple,font_fixture
from tests.kernel_font_glyph import real_info
from tests.kernel_font_cmap import STACK
BASE,DATA,BUFFER,OUTPUT,HEAP,FONT=0x800000,0x600000,0x500000,0x510000,0x2000000,0xa00000
SLOTS=[('font_edges','kernel_font_edges_831884',0x831884,0x831ba8),
 ('font_raster','kernel_font_raster_83244c',0x83244c,0x832530),
 ('font_bitmap_free','kernel_font_bitmap_free_832530',0x832530,0x832544),
 ('font_get_glyph_subpixel','kernel_font_get_glyph_subpixel_832544',0x832544,0x832700),
 ('font_get_glyph','kernel_font_get_glyph_832700',0x832700,0x832756),
 ('font_make_glyph_subpixel','kernel_font_make_glyph_subpixel_832756',0x832756,0x832852),
 ('font_make_glyph','kernel_font_make_glyph_832852',0x832852,0x8328a8),
 ('font_get_codepoint_subpixel','kernel_font_get_codepoint_subpixel_8328a8',0x8328a8,0x83290c),
 ('font_make_codepoint_subpixel','kernel_font_make_codepoint_subpixel_83298a',0x83298a,0x8329ec),
 ('font_get_codepoint','kernel_font_get_codepoint_8329ec',0x8329ec,0x832a42),
 ('font_make_codepoint','kernel_font_make_codepoint_832a42',0x832a42,0x832a98)]
GET={3,4,7,9};MAKE={5,6,8,10}

class Machine(HeapMachine):
    returns=GET;mutable_caller={3}
    def execute(self,case):
        result=super().execute(case)
        if 'edge_oracle' in case:
            expected=case['edge_oracle'];raw=bytes(self.uc.mem_read(HEAP+16,len(expected)*20))
            records=[raw[i:i+20] for i in range(0,len(raw),20)]
            assert sorted(records)==sorted(expected),(self.label,'edge records')
            keys=[struct.unpack_from('<f',record,4)[0] for record in records]
            assert keys==sorted(keys),(self.label,'edge Y order')
        return result

def edge_case(rings,sx=1,sy=1,dx=0,dy=0,invert=0,control=0x37f,limit=HEAP+0x100000,oracle=True):
    points=[p for ring in rings for p in ring]
    setup=[*heap_setup(limit),(DATA,b''.join(struct.pack('<2f',*p) for p in points)),
        (DATA+0x4000,words(*map(len,rings))),(BUFFER,words(8,0,32,OUTPUT))]
    c=fixture(0,[BUFFER,DATA,DATA+0x4000,len(rings),*map(fbits,(sx,sy,dx,dy)),0,0,invert,77],setup,control=control)
    if oracle and control==0x37f and limit==HEAP+0x100000:
        # Independently orient each polygon segment by original vertical
        # direction, transform endpoints, then stable-sort by start Y.
        edges=[]
        for ring in rings:
            for p,q in zip(ring[-1:]+ring[:-1],ring):
                if p[1]==q[1]:continue
                direction=(p[1]>q[1]) if invert else (p[1]<q[1])
                a,b=(p,q) if direction else (q,p)
                edges.append((a[0]*sx+dx,(a[1]*sy*(-1 if invert else 1)+dy)*15,
                    b[0]*sx+dx,(b[1]*sy*(-1 if invert else 1)+dy)*15,int(direction)))
        c['edge_oracle']=[struct.pack('<4fI',*e) for e in edges]
        c['expected_memory']=[(HEAP+16+len(edges)*20+4,struct.pack('<f',1))]
    return c

def edge_cases():
    rng=random.Random(0x831884);cases=[]
    ringsets=[[],[[]],[[(1,1)]],[[(0,0),(8,0),(8,4),(0,4)]],
        [[(0,0),(8,0),(8,4),(0,4)],[(2,1),(2,3),(6,3),(6,1)]],
        [[(0,-4),(4,4),(8,-4),(0,-4)]], [[(0,-1),(8,-1),(8,-1)]]]
    for rings in ringsets:
        for invert in (0,1,-1):
            for transform in [(1,1,0,0),(-1,1,8,0),(.5,.25,-1,.5),(1,-1,0,0)]:
                for control in CONTROLS:cases.append(edge_case(rings,*transform,invert,control))
    for i in range(160):
        rings=[[(rng.randrange(-64,64)/4,rng.randrange(-64,64)/4) for _ in range(i%13)] for j in range(i%4)]
        cases.append(edge_case(rings,sx=.5,sy=.25,dx=.5,dy=-.25,invert=i&1))
    for limit in (HEAP,HEAP+1024,HEAP+1025):
        for rings in ringsets:cases.append(edge_case(rings,limit=limit,oracle=False))
    for value in (math.inf,-math.inf,math.nan,-0.):
        for axis in (0,1):
            ring=[(0.,0.),(1.,2.),(4.,3.)];p=list(ring[1]);p[axis]=value;ring[1]=tuple(p)
            cases.append(edge_case([ring],oracle=False))
    for count in (-2147483648,-1):
        c=edge_case([],oracle=False);c['args'][3]=count;cases.append(c)
    # Edge output overwrites points still needed by the builder.
    for shift in (0,4,8,16):
        c=edge_case(ringsets[3],oracle=False);c['args'][1]=HEAP+16+shift
        c['setup'][1]=(HEAP+16+shift,c['setup'][1][1]);cases.append(c)
    return cases

def raster_cases():
    cases=[]
    for height in (0,1,4,7,8,9):
        shape=[(1,1,-height),(2,7,-height),(2,7,0),(2,1,0),(2,1,-height)]
        raw=b''.join(vertex(*v) for v in shape)
        for width in (0,1,8,16):
            for invert in (0,1):
                for control in CONTROLS:
                    c=fixture(1,[BUFFER,fbits(.35),DATA,len(shape),fbits(1),fbits(1),0,0,0,0,invert,77],
                        [*heap_setup(),(DATA,raw),(BUFFER,words(width,height,width*4,OUTPUT)),(OUTPUT,b'\xa5'*1024)],control=control)
                    if invert and width>=8 and height:
                        weight=255 if height<8 else 255
                        row=bytes([0]+[weight]*6+[0]+[0]*(width*4-8))
                        c['expected_memory']=[(OUTPUT,row*height)]
                    cases.append(c)
    for limit in (HEAP,HEAP+1024,HEAP+2048,HEAP+3072,HEAP+4096,HEAP+8192):
        c=fixture(1,[BUFFER,fbits(.35),DATA,5,fbits(1),fbits(1),0,0,0,0,1,77],
            [*heap_setup(limit),(DATA,b''.join(vertex(*v) for v in [(1,0,0),(2,8,0),(2,8,-4),(2,0,-4),(2,0,0)])),
                (BUFFER,words(8,4,32,OUTPUT))]);cases.append(c)
    return cases

def glyph_case(fn,info,font,glyph=0,sx=1,sy=1,dx=0,dy=0,dimensions=(8,4,32),limit=HEAP+0x100000,control=0x37f):
    if fn in GET:
        args=[BUFFER,fbits(sx),fbits(sy)]
        if fn in (3,7):args += [fbits(dx),fbits(dy)]
        args += [glyph,OUTPUT,OUTPUT+4,OUTPUT+8,OUTPUT+12]
    else:
        args=[BUFFER,OUTPUT,*dimensions,fbits(sx),fbits(sy)]
        if fn in (5,8):args += [fbits(dx),fbits(dy)]
        args += [glyph]
    return fixture(fn,args,[*heap_setup(limit),(BUFFER,info),(DATA,font),(OUTPUT,b'\xa5'*1024)],control=control)

def glyph_cases(font):
    cases=[]
    square=[(0,0,True),(8,0,True),(8,4,True),(0,4,True)]
    glyph=bytearray(simple([square],True));struct.pack_into('>5h',glyph,0,1,0,0,8,4)
    curved=bytearray(simple([[(0,0,True),(0,8,False),(8,8,True),(8,0,True)]],True));struct.pack_into('>5h',curved,0,1,0,0,8,8)
    info,raw=font_fixture([bytes(glyph),bytes(curved),b''])
    for fn in sorted(GET|MAKE):
        for g in (0,1,2):
            for sx,sy in [(1,1),(0,1),(1,0),(0,0),(.5,.25),(-.5,.25)]:
                for control in CONTROLS:
                    c=glyph_case(fn,info,raw,2 if sx<0 and fn in GET else g,sx,sy,control=control)
                    if fn==3:
                        args=c['args'][:]
                        if sx==0:args[1]=args[2]
                        if sy==0 and sx!=0:args[2]=args[1]
                        c['expected_caller']=words(*args)
                    cases.append(c)
        for limit in (HEAP,HEAP+1024,HEAP+2048,HEAP+3072,HEAP+4096,HEAP+8192):
            cases.append(glyph_case(fn,info,raw,limit=limit))
        if fn in MAKE:
            for dims in ((0,4,32),(8,0,32),(0,0,0),(-1,0,32),(1,2,4),(16,4,64)):
                cases.append(glyph_case(fn,info,raw,dimensions=dims))
        else:
            for mask in range(16):
                c=glyph_case(fn,info,raw)
                for i in range(4):
                    if mask>>i&1:c['args'][-4+i]=0
                cases.append(c)
            for delta in (0,4,8,12):
                c=glyph_case(fn,info,raw);c['args'][-4:]=[OUTPUT+delta]*4;cases.append(c)
        if fn in (3,5,7,8):
            for dx,dy in ((.25,.75),(-.5,-.25),(1,-1)):
                cases.append(glyph_case(fn,info,raw,dx=dx,dy=dy))
    # Width write changes a later output pointer and a scale word.
    for offset in (0,4):
        for target in (1,2,8,9):
            c=glyph_case(3,info,raw);c['args'][6]=STACK+offset+4+target*4
            c.update(stack_offset=offset,caller_alias=True);cases.append(c)
    # Unordered scales on empty outlines avoid undefined oversized allocations.
    for fn in sorted(GET|MAKE):
        for value in (math.nan,math.inf,-math.inf,-0.):cases.append(glyph_case(fn,info,raw,2,value,1,dimensions=(0,0,0)))
    # Real simple glyphs at a bounded pixel scale. All nested shape/heap,
    # flatten/sort/scan/raster code executes; no helper boundaries are stubbed.
    real,tables,count,fmt,_=real_info(font);chosen=[]
    for g in range(count):
        a=struct.unpack_from('>H' if fmt==0 else '>I',font,tables[b'loca']+g*(2 if fmt==0 else 4))[0]*(2 if fmt==0 else 1)
        b=struct.unpack_from('>H' if fmt==0 else '>I',font,tables[b'loca']+(g+1)*(2 if fmt==0 else 4))[0]*(2 if fmt==0 else 1)
        if a!=b and struct.unpack_from('>h',font,tables[b'glyf']+a)[0]>0 and g%37==0:chosen.append(g)
    for g in chosen:
        for fn in (3,4,5,6):
            c=glyph_case(fn,real,b'',g,1/512,1/512)
            c['setup'].append((FONT,font));cases.append(c)
    return cases,len(chosen)

def free_cases():
    cases=[fixture(2,[0,77],heap_setup())]
    for count in range(1,5):
        setup=heap_setup()
        for i in range(count):setup.append((HEAP+i*1024,words(HEAP+(i+1)*1024,HEAP+(i-1)*1024 if i else 0,1,23)))
        setup.append((HEAP+count*1024,words(0,HEAP+(count-1)*1024,0,0)))
        for i in range(count):cases.append(fixture(2,[HEAP+i*1024+16,77],setup))
    return cases

def main():
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--installed',action='store_true');args=parser.parse_args()
    started=time.monotonic();disk=Fat16((ROOT.parent/'IceStarOS_x86_32M.vhd').read_bytes());original=disk.read_kernel()
    assert hashlib.sha256(original).hexdigest()==ORIGINAL_SHA256
    variants,sizes=compile_variants(original,args.installed,'render',SLOTS)
    machines=[Machine(*spec,original,b'') for spec in [('original',original,BASE,[s for _,_,s,_ in SLOTS]),*variants]]
    font=disk.read_file(b'ICESTAR    ',b'FONT       ',b'FIRA    TTF');glyphs,real_count=glyph_cases(font)
    cases=edge_cases()+raster_cases()+free_cases()+glyphs;counts={n:0 for n,_,_,_ in SLOTS}
    for i,c in enumerate(cases):
        c.setdefault('stack_offset',4 if i&1 else 0)
        try:
            expected=machines[0].execute(c)
            for m in machines[1:]:
                actual=m.execute(c);assert actual==expected,(m.label,expected,actual)
        except Exception:
            print('Failing render case',i,{k:v for k,v in c.items() if k not in ('setup','expected_memory')},flush=True);raise
        counts[SLOTS[c['function']][0]]+=1
    summary=dict(installed=args.installed,functions=len(SLOTS),scenarios=len(cases),executions=len(cases)*3,
        slots=sizes,scenarios_by_function=counts,real_simple_fira_glyphs=real_count,seconds=round(time.monotonic()-started,3),
        checks=['real malloc/free, glyph decoder, flatten, sort, scan and raster driver',
            'all touched data/heap pages and caller words','independent oriented/transformed polygon edges',
            'independent rectangle output including DWORD-width row padding','5/15 sample threshold at height 8',
            'six x87 controls and both stack alignments','scale fallback writeback','null/aliased output pointers',
            'empty glyphs, negative scales and signed count boundaries','allocation failures at each stage',
            'real simple Fira glyph bitmap pipelines at bounded pixel scale'],
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in
            ('kernel/font_render.c','kernel/font_render.h','tests/kernel_font_render.py')})
    (BUILD/'font_render_summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))

if __name__=='__main__':main()
