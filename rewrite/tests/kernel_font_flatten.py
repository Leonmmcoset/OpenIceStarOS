"""Original/ordinary/installed curve subdivision and two-pass contour expansion."""
import argparse
from fractions import Fraction
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
from tests.kernel_font_context import fbits
from tests.kernel_font_cmap import STACK
from tests.kernel_font_glyph import real_info
from tests.kernel_font_shape_qemu import simple_geometry
BASE,DATA,BUFFER,OUTPUT,HEAP=0x800000,0x600000,0x500000,0x510000,0x2000000
SLOTS=[('font_point','kernel_font_point_831ba8',0x831ba8,0x831bdc),
 ('font_quadratic','kernel_font_quadratic_831bdc',0x831bdc,0x831d64),
 ('font_cubic','kernel_font_cubic_831d64',0x831d64,0x832000),
 ('font_flatten','kernel_font_flatten_832000',0x832000,0x83244c)]
TAILS={'font_flatten':['kernel_font_cubic_error'],'font_cubic':['kernel_font_curve_endpoint']}
CONTROLS=(0x37f,0x7f,0x27f,0x77f,0xb7f,0xf7f)

class Machine(HeapMachine):
    returns={1,3};mutable_caller=set()

def heap_setup(limit=HEAP+0x100000):
    return [(HEAP,words(0,0,0,0)),(0x1b050,words(limit)),(0x1b0d8,words(23))]

def vertex(kind,x,y,cx=0,cy=0,cx1=0,cy1=0):
    return struct.pack('<6hBB',x,y,cx,cy,cx1,cy1,kind,0xa5)

def point_cases():
    cases=[]
    values=[0,0x80000000,fbits(1.25),fbits(-8),0x7f800000,0xff800000,
        0x7fc12345,0xffc12345,0x7f800001,0xff800001]
    for index in (0,1,17,0x7fffffff,0x80000000,0xffffffff):
        address=(OUTPUT-index*8)&0xffffffff
        for i,x in enumerate(values):
            y=values[-i-1]
            cases.append(fixture(0,[address,index,x,y],memory=[(OUTPUT,words(x,y))]))
    for x in values:cases.append(fixture(0,[0,0x80000000,x,x]))
    # Original point writer reloads pointer/index/Y after its first store.
    # These aliases mutate only coordinate words in its own caller area.
    for offset in (0,4):
        for word in (2,3):
            for x in values:
                c=fixture(0,[STACK+offset+4+word*4,0,x,fbits(3)])
                c.update(stack_offset=offset,caller_alias=True);cases.append(c)
        c=fixture(0,[STACK+offset+4,0,OUTPUT,fbits(3)],memory=[(OUTPUT+4,words(fbits(3)))])
        c.update(stack_offset=offset,caller_alias=True);cases.append(c)
    return cases

def quadratic_points(coords,tolerance,depth=0):
    # Closed Bernstein polynomial at the uniform dyadic parameters. A
    # quadratic's midpoint deviation scales by 1/16 after each split.
    x0,y0,x1,y1,x2,y2=map(Fraction,coords)
    error=((x0-2*x1+x2)**2+(y0-2*y1+y2)**2)/16
    if depth>16:return []
    splits=0
    while error>tolerance:
        splits+=1;error/=16
        if depth+splits>16:return []
    result=[]
    for i in range(1,(1<<splits)+1):
        t=Fraction(i,1<<splits);u=1-t
        result.append((float(u*u*x0+2*u*t*x1+t*t*x2),float(u*u*y0+2*u*t*y1+t*t*y2)))
    return result

def curve_case(fn,coords,tolerance,depth=0,points=OUTPUT,count=0,control=0x37f,oracle=None):
    setup=[(BUFFER,words(count)),(OUTPUT,bytes([0xa5])*4096)]
    args=[points,BUFFER,*map(fbits,coords),fbits(tolerance),depth]
    memory=[]
    if oracle is not None:
        memory=[(BUFFER,words(count+len(oracle)))]
        if points and oracle:memory.append(((points+count*8)&0xffffffff,b''.join(struct.pack('<2f',*p) for p in oracle)))
    return fixture(fn,args,setup,1 if fn==1 else None,memory,control)

def curve_cases():
    cases=[];rng=random.Random(0x831bdc)
    for coords in [(0,0,0,8,8,8),(0,0,4,4,8,8),(-8,4,0,-4,8,4)]:
        for tolerance in (Fraction(1,8),Fraction(1,2),1,8,64,1024):
            for depth in (0,15,16,17,2147483647):
                expected=quadratic_points(coords,tolerance,depth)
                for points in (0,OUTPUT):cases.append(curve_case(1,coords,tolerance,depth,points,oracle=expected))
    cubic=(0,0,0,8,8,8,8,0)
    for tolerance,oracle in [(64,[(4,6),(8,0)]),(1024,[(8,0)])]:
        for points in (0,OUTPUT):cases.append(curve_case(2,cubic,tolerance,points=points,oracle=oracle))
    for fn,size in [(1,6),(2,8)]:
        for i in range(100):
            coords=[rng.randrange(-1024,1024)/8 for _ in range(size)]
            for control in CONTROLS:
                cases.append(curve_case(fn,coords,(.125,.5,1,64,1024)[i%5],i%3,control=control))
        for bits in (0,0x80000000,0x7f800000,0xff800000,0x7fc12345,0xffc54321,0x7f800001,0xff800001):
            for index in range(size):
                c=curve_case(fn,[0]*size,1)
                c['args'][2+index]=bits;cases.append(c)
        for tolerance in (-1,0,math.nan,math.inf,-math.inf):
            for depth in (15,16,17):
                for control in CONTROLS:
                    cases.append(curve_case(fn,[0,0,0,8,8,8]+([8,0] if fn==2 else []),tolerance,depth,control=control))
        # Counter wrap and data aliases; use a leaf so no unbounded traversal.
        for count in (1,17,0x7fffffff,0x80000000,0xffffffff):
            endpoint=[0]*(size-2)+[3,4]
            cases.append(curve_case(fn,endpoint,math.inf,count=count,points=(OUTPUT-count*8)&0xffffffff,oracle=[(3,4)]))
        for delta in (0,4,8):
            c=curve_case(fn,[0]*size,1,points=BUFFER-delta)
            cases.append(c)
        # Counter writes to X0 are live to the second recursive half.
        for offset in (0,4):
            c=curve_case(fn,[0,0,0,8,8,8]+([8,0] if fn==2 else []),1,depth=15)
            c['args'][1]=STACK+offset+12;c.update(stack_offset=offset,caller_alias=True);cases.append(c)
            # A point overwrites the caller's count pointer. The subsequent
            # increment must use its new value rather than the old pointer.
            c=curve_case(fn,[0]*size,1,points=STACK+offset+8)
            c['args'][size]=BUFFER+4
            c['setup'][0]=(BUFFER,words(0,17))
            c.update(stack_offset=offset,caller_alias=True)
            c['expected_memory']=[(BUFFER,words(0,18))];cases.append(c)
    return cases

def flatten_case(vertices,tolerance,limit=HEAP+0x100000,lengths=OUTPUT,contours=OUTPUT+4,count=None):
    raw=b''.join(vertex(*v) for v in vertices)
    c=fixture(3,[DATA,len(vertices) if count is None else count,fbits(tolerance),lengths,contours,77],
        [(DATA,raw),*heap_setup(limit)])
    # Independent expected result for line-only shapes and no-contour paths.
    starts=[i for i,v in enumerate(vertices) if v[0]==1]
    if count is not None or not starts:
        c['expected_return']=0;c['expected_memory']=[(contours,words(0))]
    elif lengths==OUTPUT and contours==OUTPUT+4 and limit==HEAP+0x100000 and all(v[0] in (1,2,0,5,255) for v in vertices):
        points=[(v[1],v[2]) for v in vertices if v[0] in (1,2)]
        groups=[sum(v[0] in (1,2) for v in vertices[a:b]) for a,b in zip(starts,starts[1:]+[len(vertices)])]
        c['expected_return']=HEAP+1040
        c['expected_memory']=[(OUTPUT,words(HEAP+16,len(starts))),
            (HEAP+16,words(*groups)),(HEAP+1040,b''.join(struct.pack('<2f',*p) for p in points))]
    return c

def flatten_cases(font):
    cases=[];rng=random.Random(0x832000)
    square=[(1,0,0),(2,8,0),(2,8,8),(2,0,8),(2,0,0)]
    sets=[[],[(2,8,4)],[(1,0,0)],square,square+square,
        [(1,0,0),(3,8,8,0,8),(2,0,0)],[(1,0,0),(4,8,0,0,8,8,8),(2,0,0)],
        [(2,-1,-1),(1,0,0),(0,1,2),(5,8,4),(255,1,1),(2,8,4)]]
    for i in range(100):
        path=[]
        for j in range(1+i%3):
            path.append((1,rng.randrange(-64,64),rng.randrange(-64,64)))
            for k in range(1+i%5):path.append((rng.choice((2,3,4)),*[rng.randrange(-128,128) for _ in range(6)]))
        sets.append(path)
    for vertices in sets:
        for tolerance in (.125,1,8,math.inf,math.nan):cases.append(flatten_case(vertices,tolerance))
    for limit in (HEAP,HEAP+1024,HEAP+1025,HEAP+2048,HEAP+2049):
        for vertices in sets[:8]:cases.append(flatten_case(vertices,1,limit))
    for count in (-2147483648,-1,0):cases.append(flatten_case(square,1,count=count))
    for contours in (OUTPUT,OUTPUT+4,DATA+4,DATA+16):
        for lengths in (OUTPUT,OUTPUT+8):
            c=flatten_case(square,1,lengths=lengths,contours=contours)
            c.pop('expected_return',None);c['expected_memory']=[];cases.append(c)
    # Decode every simple Fira outline independently from its TrueType table.
    # Compound joins' original heap corruption belongs to the shape tests.
    _,tables,count,fmt,_=real_info(font);simple_count=0
    for glyph in range(count):
        location=struct.unpack_from('>H' if fmt==0 else '>I',font,tables[b'loca']+glyph*(2 if fmt==0 else 4))[0]*(2 if fmt==0 else 1)
        end=struct.unpack_from('>H' if fmt==0 else '>I',font,tables[b'loca']+(glyph+1)*(2 if fmt==0 else 4))[0]*(2 if fmt==0 else 1)
        if end==location or struct.unpack_from('>h',font,tables[b'glyf']+location)[0]<=0:continue
        cases.append(flatten_case(simple_geometry(font,tables,fmt,glyph),8));simple_count+=1
    return cases,simple_count

def main():
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--installed',action='store_true');args=parser.parse_args()
    started=time.monotonic();disk=Fat16((ROOT.parent/'IceStarOS_x86_32M.vhd').read_bytes());original=disk.read_kernel()
    assert hashlib.sha256(original).hexdigest()==ORIGINAL_SHA256
    variants,sizes=compile_variants(original,args.installed,'flatten',SLOTS,tails=TAILS)
    machines=[Machine(*spec,original,b'') for spec in [('original',original,BASE,[s for _,_,s,_ in SLOTS]),*variants]]
    font=disk.read_file(b'ICESTAR    ',b'FONT       ',b'FIRA    TTF')
    flattened,real_count=flatten_cases(font);cases=point_cases()+curve_cases()+flattened;counts={n:0 for n,_,_,_ in SLOTS}
    for i,c in enumerate(cases):
        c.setdefault('stack_offset',4 if i&1 else 0)
        try:
            expected=machines[0].execute(c)
            for m in machines[1:]:
                actual=m.execute(c);assert actual==expected,(m.label,expected,actual)
        except Exception:
            print('Failing flatten case',i,{k:v for k,v in c.items() if k not in ('setup','expected_memory')},flush=True);raise
        counts[SLOTS[c['function']][0]]+=1
    summary=dict(installed=args.installed,functions=len(SLOTS),scenarios=len(cases),executions=len(cases)*3,
        slots=sizes,scenarios_by_function=counts,fira_simple_outlines=real_count,seconds=round(time.monotonic()-started,3),
        checks=['real malloc/free/sqrt helpers','all touched data and heap pages','exact Bernstein quadratic endpoints',
            'independent line contours and cubic midpoint','all simple Fira outlines from independent table parser',
            'six x87 controls','both incoming stack alignments','raw signaling/quiet NaN endpoints','depth 16/17 cutoff',
            'caller/data aliases','counter wrap','both allocation failures and free/merge'],
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in
            ('kernel/font_flatten.c','kernel/font_flatten.h','tests/kernel_font_flatten.py','tests/kernel_font_outline.py')})
    (BUILD/'font_flatten_summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))

if __name__=='__main__':main()
