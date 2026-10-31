"""Font object arenas with the real original/installed kernel heap allocator."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import time
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(BUILD/'test-deps')]
from tools.kernel_image import Fat16,ORIGINAL_SHA256
from tests.kernel_font_outline import Machine as OutlineMachine,compile_variants,fixture,words
BASE,DATA,BUFFER,OUTPUT,HEAP=0x800000,0x600000,0x500000,0x510000,0x2000000
SLOTS=[('font_heap_alloc','kernel_font_heap_alloc_830b4c',0x830b4c,0x830c0e),
 ('font_heap_free','kernel_font_heap_free_830c0e',0x830c0e,0x830c28),
 ('font_heap_cleanup','kernel_font_heap_cleanup_830c28',0x830c28,0x830c5a)]

class Machine(OutlineMachine):
    aggregates=set();returns={0};mutable_caller=set()
    def __init__(self,*args):
        super().__init__(*args);self.uc.mem_map(HEAP,0x100000)

def scenarios():
    cases=[]
    base=[(HEAP,words(0,0,0,0)),(0x1b050,words(HEAP+0x100000)),(0x1b0d8,words(23))]
    for size in (0,1,4,20,31,32,33,127,128,129,1024,65535,0x7fffffff,0x80000000,0xffffffff):
        count=2000 if size<32 else 800 if size<128 else 100
        span=((size*count+4+0x40f)&0xfffffc00)
        for limit in (HEAP+0x100000,HEAP+1024):
            setup=[*base,(0x1b050,words(limit)),(BUFFER,words(0,0,0))]
            success=(HEAP+span)&0xffffffff < limit
            # Wrapped allocations with a non-forward next pointer are parity
            # cases; only ordinary bounded forward results get an oracle.
            if success and span>=1024:
                result=(HEAP+16+(count-1)*size+4)&0xffffffff
                memory=[(BUFFER,words(HEAP+16,0,count-1)),(HEAP+16,words(0))]
                cases.append(fixture(0,[BUFFER,size,99],setup,result,memory))
            elif not success:cases.append(fixture(0,[BUFFER,size,99],setup,0,[(BUFFER,words(0,0,0))]))
            else:cases.append(fixture(0,[BUFFER,size,99],setup))
        # Existing chunk: decrement only; neither size nor pointer validated.
        for remain in (1,2,17,0x80000000,0xffffffff):
            setup=[*base,(BUFFER,words(DATA,0,remain))]
            result=(DATA+(remain-1)*size+4)&0xffffffff
            cases.append(fixture(0,[BUFFER,size,77],setup,result,[(BUFFER+8,words(remain-1))]))
        # Pop free object before consulting chunk count or object size.
        for result in (OUTPUT,BUFFER,BUFFER+4,BUFFER+8):
            setup=[*base,(BUFFER,words(DATA,result,17)),(result,words(DATA+64))]
            cases.append(fixture(0,[BUFFER,size,0],setup))
    for obj in (OUTPUT,BUFFER,BUFFER+4,BUFFER+8,DATA+1):
        for old in (0,DATA,OUTPUT):
            setup=[(BUFFER,words(DATA,old,17))]
            memory=[] if obj in (BUFFER,BUFFER+4,BUFFER+8) else [(obj,words(old)),(BUFFER+4,words(obj))]
            cases.append(fixture(1,[BUFFER,obj],setup,memory=memory))
    # Real malloc headers: three adjacent live allocations and a sentinel.
    # Cleanup caches next before real free/merge; arena fields remain stale.
    for chunks in range(4):
        addresses=[HEAP+i*1024+16 for i in range(chunks)]
        setup=[*base,(BUFFER,words(addresses[0] if addresses else 0,OUTPUT,7))]
        for i,address in enumerate(addresses):
            setup += [(address-16,words(address-16+1024,address-16-1024 if i else 0,1,23)),
                (address,words(addresses[i+1] if i+1<chunks else 0))]
        setup += [(HEAP+chunks*1024,words(0,HEAP+(chunks-1)*1024 if chunks else 0,0,0))]
        c=fixture(2,[BUFFER,99],setup,memory=[(BUFFER,words(addresses[0] if addresses else 0,OUTPUT,7))])
        if chunks:c['expected_memory'] += [(HEAP,words(HEAP+2048 if chunks==3 else 0,0,0,0))]
        cases.append(c)
    return cases

def main():
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--installed',action='store_true');args=parser.parse_args()
    started=time.monotonic();original=Fat16((ROOT.parent/'IceStarOS_x86_32M.vhd').read_bytes()).read_kernel()
    assert hashlib.sha256(original).hexdigest()==ORIGINAL_SHA256
    variants,sizes=compile_variants(original,args.installed,'heap',SLOTS)
    machines=[Machine(*spec,original,b'') for spec in [('original',original,BASE,[s for _,_,s,_ in SLOTS]),*variants]]
    cases=scenarios();counts={n:0 for n,_,_,_ in SLOTS}
    for i,c in enumerate(cases):
        try:
            expected=machines[0].execute(c)
            for m in machines[1:]:
                actual=m.execute(c);assert actual==expected,(m.label,expected,actual)
        except Exception:
            print('Failing heap case',i,{k:v for k,v in c.items() if k!='setup'},flush=True);raise
        counts[SLOTS[c['function']][0]]+=1
    summary=dict(installed=args.installed,functions=len(SLOTS),scenarios=len(cases),executions=len(cases)*3,
        slots=sizes,scenarios_by_function=counts,seconds=round(time.monotonic()-started,3),
        checks=['real original and installed malloc/free','chunk sizing thresholds','allocation failures',
            'free-list and descriptor aliases','wrapped indices','all touched pages including heap headers','cleanup and merge'],
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ('kernel/font_heap.c','kernel/font_heap.h','tests/kernel_font_heap.py','tests/kernel_font_outline.py')})
    (BUILD/'font_heap_summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
if __name__=='__main__':main()
