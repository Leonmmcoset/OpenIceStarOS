"""Original/ordinary/installed scan coverage, active edges and edge ordering."""
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
from tests.kernel_font_context import fbits
BASE,DATA,BUFFER,OUTPUT,HEAP=0x800000,0x600000,0x500000,0x510000,0x2000000
SLOTS=[('font_active','kernel_font_active_830c5a',0x830c5a,0x830dc4),
 ('font_fill_scanline','kernel_font_fill_scanline_830dc4',0x830dc4,0x830f24),
 ('font_edge_insertion','kernel_font_edge_insertion_831340',0x831340,0x831480),
 ('font_edge_quicksort','kernel_font_edge_quicksort_831480',0x831480,0x831858),
 ('font_edge_sort','kernel_font_edge_sort_831858',0x831858,0x831884)]

class Machine(HeapMachine):
    returns={0};mutable_caller={1,3}

def heap_setup(limit=HEAP+0x100000):
    return [(HEAP,words(0,0,0,0)),(0x1b050,words(limit)),(0x1b0d8,words(23))]

def f32(value):return struct.unpack('<f',struct.pack('<f',value))[0]

def active_cases():
    cases=[];rng=random.Random(0x830c5a)
    values=[(0.,0.,0.,1.),(4.,0.,1.,2.),(-4.,-2.,7.,8.),(1.25,-.5,6.75,3.5),
        (-.0,0.,0.,0.),(1.,1.,2.,1.),(math.inf,0.,1.,2.),(math.nan,0.,1.,2.)]
    values += [tuple(rng.randrange(-256,256)/4 for _ in range(4)) for i in range(96)]
    for i,coords in enumerate(values):
        for direction in (0,1,0xffffffff):
            edge=struct.pack('<4fI',*coords,direction);start=.5;offset=(i%7)-3
            for control in (0x37f,0x7f,0x27f,0x77f,0xb7f,0xf7f):
                setup=[*heap_setup(),(BUFFER,words(0,OUTPUT,17)),(DATA,edge),(OUTPUT,words(0)+b'\xa5'*16)]
                memory=[]
                if control==0x37f and all(math.isfinite(v) for v in coords) and coords[3]!=coords[1]:
                    x0,y0,x1,y1=coords;slope=f32((x1-x0)/(y1-y0))
                    dx=-math.floor(-slope*1024) if slope<0 else math.floor(slope*1024)
                    x=math.floor(x0*1024+dx*(start-y0))-(offset<<10)
                    memory=[(OUTPUT,words(0,x,dx,fbits(y1),1 if direction else -1))]
                cases.append(fixture(0,[BUFFER,DATA,offset,fbits(start),77],setup,OUTPUT,memory,control))
    for limit in (HEAP,HEAP+1024,HEAP+0x100000):
        for coords in values[:8]:
            setup=[*heap_setup(limit),(BUFFER,words(0,0,0)),(DATA,struct.pack('<4fI',*coords,1))]
            result=0 if limit<=HEAP+40960 else HEAP+40000
            cases.append(fixture(0,[BUFFER,DATA,0,fbits(.5),0],setup,result))
    # Arena object aliases the original edge. Subsequent raw reads must see
    # the writes to dx/x/next, rather than a cached edge structure.
    for delta in (0,4,8,12,16):
        setup=[*heap_setup(),(BUFFER,words(0,DATA+delta,17)),(DATA,struct.pack('<4fI',1.,0.,3.,2.,1)+bytes(32))]
        cases.append(fixture(0,[BUFFER,DATA,0,fbits(.5),0],setup,DATA+delta))
    for offset in (-2147483648,-2097153,-1,0,1,2097152,2147483647):
        setup=[*heap_setup(),(BUFFER,words(0,OUTPUT,17)),(DATA,struct.pack('<4fI',1.25,-.5,6.75,3.5,1)),(OUTPUT,words(0)+bytes(16))]
        cases.append(fixture(0,[BUFFER,DATA,offset,fbits(.5),0],setup,OUTPUT,
            [(OUTPUT,words(0,2688-(offset<<10),1408,fbits(3.5),1))]))
    return cases

def coverage(width,intervals,weight,initial):
    # Independent intersection lengths, one pixel at a time, for sorted
    # fixed-point crossings. Edge traversal and clipping are not duplicated.
    result=bytearray(initial);winding=0;start=None;spans=[]
    for position,direction in intervals:
        before=winding;winding+=direction
        if before==0 and winding!=0:start=position
        if before!=0 and winding==0:spans.append((start,position))
    for pixel in range(max(width,0)):
        for left,right in spans:
            length=max(0,min(right,(pixel+1)*1024)-max(left,pixel*1024))
            result[pixel]=(result[pixel]+length*weight//1024)&255
    return bytes(result)

def fill_case(width,crossings,weight,initial=None,scanline=OUTPUT,oracle=True):
    initial=bytes([0xa5])*max(width,32) if initial is None else initial
    setup=[(scanline,initial)]
    for i,(x,direction) in enumerate(crossings):
        setup.append((DATA+i*32,words(DATA+(i+1)*32 if i+1<len(crossings) else 0,x,0,fbits(10.),direction)))
    args=[scanline,width,DATA if crossings else 0,weight]
    memory=[(scanline,coverage(width,crossings,weight,initial))] if oracle else []
    c=fixture(1,args,setup,memory=memory);c['expected_caller']=words(scanline,width,0,weight)
    return c

def fill_cases():
    cases=[];rng=random.Random(0x830dc4)
    fixed=[[],[(0,1),(1024,-1)],[(256,1),(768,-1)],[(-512,1),(8192,-1)],
        [(256,1),(1536,1),(3072,-1),(4096,-1)],[(256,1),(1536,-1),(2048,1),(3584,-1)],
        [(0,0),(2048,1),(2048,-1)],[(1024,1)],[(1024,-1),(2048,1)]]
    for crossings in fixed:
        for width in (-1,0,1,4,8,32):
            for weight in (0,1,17,63,255):cases.append(fill_case(width,crossings,weight))
    for i in range(320):
        xs=sorted(rng.randrange(-4096,40960) for _ in range(2*(1+i%7)))
        crossings=[(x,1 if j%2==0 else -1) for j,x in enumerate(xs)]
        width=1+i%32;weight=i%256
        cases.append(fill_case(width,crossings,weight,bytes(rng.randrange(256) for _ in range(max(width,32)))))
    for weight in (-2147483648,-1,256,1024,0x7fffffff):
        for crossings in fixed:cases.append(fill_case(32,crossings,weight,oracle=False))
    for directions in ((0x80000000,0x80000000),(0x7fffffff,1,0x80000000),
                       (0xffffffff,0xffffffff,2),(0x7fffffff,0x80000001)):
        cases.append(fill_case(8,[(256+i*1024,d) for i,d in enumerate(directions)],63,oracle=False))
    # Overlapping output and a later crossing's X field changes its live read.
    for shift in (0,1,2,3):
        cases.append(fill_case(4,fixed[5],63,scanline=DATA+3*32+4+shift,oracle=False))
    return cases

def sorting_cases():
    cases=[];rng=random.Random(0x831480)
    for count in (0,1,2,3,7,11,12,13,14,15,23,24,25,31,32,63,64,127,128):
        patterns=[list(range(count)),list(reversed(range(count))),[0]*count,
            [i%3 for i in range(count)],[rng.randrange(-10000,10000)/16 for _ in range(count)],
            [(-1)**i*i for i in range(count)],
            [rng.choice((math.nan,math.inf,-math.inf,-0.,0.,1.,-1.)) for _ in range(count)]]
        for keys in patterns:
            records=[struct.pack('<IfIII',i,key,0xa5a50000+i,0x5a5a0000+i,i%2) for i,key in enumerate(keys)]
            raw=b''.join(records)
            for fn in (2,3,4):
                memory=[]
                if fn==2 and all(math.isfinite(k) for k in keys):
                    memory=[(DATA,b''.join(records[i] for i in sorted(range(count),key=lambda i:keys[i])))]
                c=fixture(fn,[DATA,count],[(DATA,raw)],memory=memory)
                c['sort_records']=records;c['sort_complete']=fn in (2,4);c['sort_finite']=all(math.isfinite(k) for k in keys)
                cases.append(c)
    for fn in (2,3,4):
        for count in (-2147483648,-1):cases.append(fixture(fn,[DATA,count]))
        # Raw signaling/quiet NaN payloads and infinities must survive every
        # record move; float comparisons may not rewrite their stored bits.
        bits=(0x7f800001,0x7fc12345,0xff800001,0xffc54321,0x80000000,0,0x7f800000,0xff800000)
        raw=b''.join(words(i,bits[i%len(bits)],i+100,i+200,i%2) for i in range(25))
        c=fixture(fn,[DATA,25],[(DATA,raw)])
        c.update(sort_records=[raw[i*20:i*20+20] for i in range(25)],sort_complete=fn in (2,4),sort_finite=False)
        cases.append(c)
    return cases

def main():
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--installed',action='store_true');args=parser.parse_args()
    started=time.monotonic();original=Fat16((ROOT.parent/'IceStarOS_x86_32M.vhd').read_bytes()).read_kernel()
    assert hashlib.sha256(original).hexdigest()==ORIGINAL_SHA256
    variants,sizes=compile_variants(original,args.installed,'scan',SLOTS)
    machines=[Machine(*spec,original,b'') for spec in [('original',original,BASE,[s for _,_,s,_ in SLOTS]),*variants]]
    cases=active_cases()+fill_cases()+sorting_cases();counts={n:0 for n,_,_,_ in SLOTS}
    for i,c in enumerate(cases):
        c['stack_offset']=4 if i&1 else 0
        try:
            expected=machines[0].execute(c)
            for m in machines[1:]:
                actual=m.execute(c);assert actual==expected,(m.label,expected,actual)
            if 'sort_records' in c:
                count=len(c['sort_records']);raw=bytes(machines[0].uc.mem_read(DATA,count*20))
                records=[raw[j*20:j*20+20] for j in range(count)]
                assert sorted(records)==sorted(c['sort_records']),'edge permutation changed'
                if c['sort_complete'] and c['sort_finite']:
                    keys=[struct.unpack_from('<f',record,4)[0] for record in records]
                    assert keys==sorted(keys),'edge order'
        except Exception:
            print('Failing scan case',i,{k:v for k,v in c.items() if k not in ('setup','sort_records')},flush=True);raise
        counts[SLOTS[c['function']][0]]+=1
    summary=dict(installed=args.installed,functions=len(SLOTS),scenarios=len(cases),executions=len(cases)*3,
        slots=sizes,scenarios_by_function=counts,seconds=round(time.monotonic()-started,3),
        checks=['real arena/malloc/floor helpers','all touched data/heap pages','independent interval-pixel intersections',
            'independent active-edge coordinates','stable insertion order and complete edge permutations','quicksort 12/13 cutoff',
            'both incoming stack alignments','six x87 controls','NaNs, infinities, signed zero','caller argument writeback',
            'clipped/nonzero-winding coverage and byte wrap','output/edge aliases'],
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ('kernel/font_scan.c','kernel/font_scan.h','tests/kernel_font_scan.py')})
    (BUILD/'font_scan_summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))

if __name__=='__main__':main()
