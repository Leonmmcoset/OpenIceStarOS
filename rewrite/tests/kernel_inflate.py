"""DEFLATE bit/tree/block/zlib recovery against original, with RFC-derived fixtures."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import sys
import time
import zlib
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(BUILD/'test-deps')]
from tools.kernel_image import Fat16,ORIGINAL_SHA256
from tests.kernel_font_outline import compile_variants,fixture,words
from tests.kernel_font_heap import Machine as HeapMachine
from tests.kernel_png import context
BASE,DATA,BUFFER,OUTPUT,HEAP=0x800000,0x600000,0x500000,0x510000,0x2000000
POS,WRITTEN,TREE,NODES,LENGTHS,DIST,CODE=BUFFER+128,BUFFER+132,BUFFER+256,OUTPUT+0x10000,DATA+0x10000,BUFFER+288,BUFFER+320
SLOTS=[
    ('png_bit', 'kernel_png_bit_8380ef', 8618223, 8618275),
    ('png_bits', 'kernel_png_bits_838123', 8618275, 8618349),
    ('png_tree_init', 'kernel_png_tree_init_83816d', 8618349, 8618381),
    ('png_tree_build', 'kernel_png_tree_build_83818d', 8618381, 8619289),
    ('png_symbol', 'kernel_png_symbol_838519', 8619289, 8619487),
    ('png_dynamic', 'kernel_png_dynamic_8385df', 8619487, 8620995),
    ('png_compressed', 'kernel_png_compressed_838bc3', 8620995, 8621999),
    ('png_stored', 'kernel_png_stored_838faf', 8621999, 8622317),
    ('png_inflate', 'kernel_png_inflate_8390ed', 8622317, 8622691),
    ('png_zlib', 'kernel_png_zlib_839263', 8622691, 8623009),
]
INDEX={n.removeprefix('png_'):i for i,(n,_,_,_) in enumerate(SLOTS)}
class Machine(HeapMachine):
    returns={INDEX[n] for n in ('bit','bits','symbol','inflate','zlib')};mutable_caller=set()
def case(name,args,setup=(),expected=None,memory=()):return fixture(INDEX[name],args,setup,expected,memory)
def tree_model(lengths):
    # Canonical codes (RFC1951); independently insert their MSB-first paths.
    maximum=max(lengths,default=0);counts=[lengths.count(i) for i in range(maximum+1)];codes={};value=0
    for bits in range(1,maximum+1):
        value=(value+(counts[bits-1] if bits>1 else 0))*2
        for symbol,length in enumerate(lengths):
            if length==bits:codes[symbol]=(value,bits);value+=1
        value-=counts[bits]
    nodes=[0x7fff]*(len(lengths)*2);nextnode=0
    for symbol,(code,bits) in sorted(codes.items()):
        n=0
        for j in range(bits):
            b=(code>>(bits-j-1))&1;i=n*2+b
            if nodes[i]==0x7fff:
                if j+1==bits:nodes[i]=symbol;n=0
                else:nextnode+=1;nodes[i]=len(lengths)+nextnode;n=nextnode
            else:n=nodes[i]-len(lengths)
    return [0 if n==0x7fff else n for n in nodes],codes
class Bits:
    def __init__(self):self.bits=[]
    def put(self,value,count):self.bits.extend((value>>i)&1 for i in range(count))
    def huffman(self,value,count):self.bits.extend((value>>(count-i-1))&1 for i in range(count))
    def data(self):
        return bytes(sum(b<<j for j,b in enumerate(self.bits[i:i+8])) for i in range(0,len(self.bits),8))
def fixed_stream(symbols,distances=()):
    lengths=[8]*144+[9]*112+[7]*24+[8]*8;_,codes=tree_model(lengths)
    bits=Bits();bits.put(1,1);bits.put(1,2);ds=iter(distances)
    for sym in symbols:
        bits.huffman(*codes[sym])
        if 257<=sym<=285:
            # Length257/285 have zero extra bits; callers supply these only.
            d=next(ds);bits.huffman(d,5)
    return bits.data()
def dynamic_stream():
    # Four explicit code-length codes: 0,1,17,18. Their symbols encode 65
    # zero lengths, A length1, 190 zero lengths, EOB length1, distance length1.
    bits=Bits();bits.put(1,1);bits.put(2,2);bits.put(0,5);bits.put(0,5);bits.put(14,4)
    order=[16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1]
    lengths=[0]*19
    for n in (0,1,17,18):lengths[n]=2
    for n in order:bits.put(lengths[n],3)
    _,codes=tree_model(lengths)
    def emit(s,extra=0,count=0):bits.huffman(*codes[s]);bits.put(extra,count)
    emit(18,54,7);emit(1);emit(18,127,7);emit(18,41,7);emit(1);emit(1)
    header=len(bits.bits)
    for _ in range(7):bits.huffman(0,1)
    bits.huffman(1,1)
    stream=bits.data();assert zlib.decompress(stream,-15)==b'A'*7
    return stream,header

def dynamic_repeats():
    # A complete tree with length1 EOB and eight length4 literals. Its
    # header explicitly exercises repeat-previous16 and zero repeats17/18.
    bits=Bits();bits.put(1,1);bits.put(2,2);bits.put(0,5);bits.put(0,5);bits.put(14,4)
    order=[16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1]
    lengths=[0]*19
    for n in (0,1,2,3,4,16,17,18):lengths[n]=3
    for n in order:bits.put(lengths[n],3)
    _,codes=tree_model(lengths)
    def emit(symbol,value=0,count=0):bits.huffman(*codes[symbol]);bits.put(value,count)
    emit(18,54,7);emit(4);emit(16,3,2);emit(4)
    emit(18,127,7);emit(18,24,7);emit(17,7,3);emit(1);emit(1)
    # 1 literal +6 repeated +1 literal =8 length4 codes.
    literal=[0]*288;literal[65:73]=[4]*8;literal[256]=1
    header=len(bits.bits);_,codes=tree_model(literal)
    payload=b'ABCDEFGH'*2
    for b in payload:bits.huffman(*codes[b])
    bits.huffman(*codes[256]);stream=bits.data()
    assert zlib.decompress(stream,-15)==payload
    return stream,header,payload,literal


def scenarios():
    rng=random.Random(0x83818d);cases=[]
    data=bytes(rng.randrange(256) for _ in range(256))
    for bit in range(64):
        expected=(data[bit//8]>>(bit&7))&1
        cases.append(case('bit',[POS,DATA],[(POS,words(bit)),(DATA,data)],expected,[(POS,words(bit+1))]))
        for count in (0,1,2,3,7,8,9,15,16,31,32,33,40,63):
            result=0
            for i in range(count):result|=((data[(bit+i)//8]>>((bit+i)&7))&1)<<(i&31)
            cases.append(case('bits',[POS,DATA,count],[(POS,words(bit)),(DATA,data)],result,[(POS,words(bit+count))]))
    for dest in (TREE,POS,WRITTEN):
        for nodes,count,maxbits in ((NODES,288,15),(0,0,0),(0xffffffff,0xffffffff,0xffffffff)):
            cases.append(case('tree_init',[dest,nodes,count,maxbits],expected=None,memory=[(dest,words(nodes,maxbits,count))]))
    groups=[[1,1],[2]*4,[3]*8,[0,1,0,2,0,3,3,0],[8]*144+[9]*112+[7]*24+[8]*8,[5]*32,[0]*19]
    for depth in range(1,10):groups.append([depth]*(1<<min(depth,8)) if depth<=8 else [0]*100+[9]*128)
    for lengths in groups:
        nodes,codes=tree_model(lengths);setup=[(BUFFER,context()),(TREE,words(NODES,15,len(lengths))),(LENGTHS,words(*lengths))]
        cases.append(case('tree_build',[BUFFER,TREE,LENGTHS],setup,memory=[(NODES,words(*nodes))]))
        for symbol in list(codes)[::max(1,len(codes)//30)]:
            for start in (0,1,3,7,8):
                bits=Bits();bits.put(0,start);bits.huffman(*codes[symbol]);blob=bits.data()+bytes(8)
                cases.append(case('symbol',[BUFFER,DATA,POS,TREE,len(blob)],[(BUFFER,context()),(TREE,words(NODES,15,len(lengths))),(NODES,words(*nodes)),(DATA,blob),(POS,words(start))],symbol,[(POS,words(start+codes[symbol][1]))]))
    # Oversubscribed but bounded canonical trees: retain original tree error.
    for lengths in ([1,1,1],[1]*19,[2]*9,[3]*19):
        cases.append(case('tree_build',[BUFFER,TREE,LENGTHS],[(BUFFER,context()),(TREE,words(NODES,15,len(lengths))),(LENGTHS,words(*lengths))]))
    for position in (0,1,7,8,16,24,32):
        for size in (0,1,2):
            for nodes in (words(1,0),words(0xffffffff,0xffffffff),words(2,2,0,1)):
                cases.append(case('symbol',[BUFFER,DATA,POS,TREE,size],[(BUFFER,context()),(TREE,words(NODES,15,2)),(NODES,nodes),(DATA,b'\0'*64),(POS,words(position))]))
    streams=[b'',b'A',b'ABC',b'A'*500,b'ABCD'*250,bytes(range(256))*3,bytes(rng.randrange(256) for _ in range(200)),b'\0'*4096]
    for payload in streams:
        for level,strategy in ((0,0),(6,zlib.Z_FIXED),(6,0),(9,zlib.Z_HUFFMAN_ONLY)):
            obj=zlib.compressobj(level,zlib.DEFLATED,15,8,strategy);wrapped=obj.compress(payload)+obj.flush();raw=wrapped[2:-4]
            assert zlib.decompress(wrapped)==payload
            for spare in (1,16,511):
                capacity=len(payload)+spare
                for name,blob,args in [('zlib',wrapped,[BUFFER,OUTPUT,capacity,DATA,len(wrapped)]),('inflate',raw+b'\0',[BUFFER,OUTPUT,capacity,DATA,len(raw)+1,0])]:
                    cases.append(case(name,args,[(BUFFER,context()),(DATA,blob+bytes(16)),(OUTPUT,bytes([0xa5])*(capacity+16))],0,[(OUTPUT,payload+bytes([0xa5])*(capacity+16-len(payload)))]))
            for capacity in (0,1,len(payload)//2,len(payload)):
                cases.append(case('zlib',[BUFFER,OUTPUT,capacity,DATA,len(wrapped)],[(BUFFER,context()),(DATA,wrapped+bytes(16)),(OUTPUT,bytes([0xa5])*(len(payload)+64))]))
    dynamic,header=dynamic_stream()
    nodes_setup=[(BUFFER,context()),(TREE,words(NODES,15,288)),(DIST,words(NODES+0x1000,15,32)),(CODE,words(NODES+0x2000,7,19)),(POS,words(3)),(DATA,dynamic+bytes(16))]
    lit=[0]*288;lit[65]=lit[256]=1;distance=[1]+[0]*31
    cases.append(case('dynamic',[BUFFER,TREE,DIST,CODE,DATA,POS,len(dynamic)],nodes_setup,memory=[(POS,words(header)),(NODES,words(*tree_model(lit)[0])),(NODES+0x1000,words(*tree_model(distance)[0]))]))
    for capacity in (6,7,8,100):
        cases.append(case('compressed',[BUFFER,OUTPUT,capacity,DATA,POS,WRITTEN,len(dynamic),2],[(BUFFER,context()),(DATA,dynamic+bytes(16)),(POS,words(3)),(WRITTEN,words(0))],memory=[(OUTPUT,b'A'*min(7,capacity))]))
    repeated,repeat_header,payload,literal=dynamic_repeats()
    setup=[(BUFFER,context()),(TREE,words(NODES,15,288)),(DIST,words(NODES+0x1000,15,32)),(CODE,words(NODES+0x2000,7,19)),(POS,words(3)),(DATA,repeated+bytes(16))]
    cases.append(case('dynamic',[BUFFER,TREE,DIST,CODE,DATA,POS,len(repeated)],setup,memory=[(POS,words(repeat_header)),(NODES,words(*tree_model(literal)[0]))]))
    for capacity in (len(payload),len(payload)+1,1024):
        cases.append(case('inflate',[BUFFER,OUTPUT,capacity,DATA,len(repeated),0],[(BUFFER,context()),(DATA,repeated+bytes(16))],0,[(OUTPUT,payload)]))
    # Explicit length/distance symbols test cyclic source repetition and the
    # strict >=capacity check; reject distance30 without silently fixing it.
    for symbols,ds,payload in [([65,257,256],[0],b'AAAA'),([65,66,257,256],[1],b'ABABA'),([65,285,256],[0],b'A'*259),([65,257,256],[30],None)]:
        raw=fixed_stream(symbols,ds)
        if payload is not None:assert zlib.decompress(raw,-15)==payload
        for capacity in (0,1,3,4,5,259,260,512):
            for prefix in (0,5):
                c=case('compressed',[BUFFER,OUTPUT,capacity,DATA,POS,WRITTEN,len(raw),1],[(BUFFER,context()),(DATA,raw+bytes(16)),(POS,words(3)),(WRITTEN,words(prefix)),(OUTPUT,bytes([0xa5])*1024)])
                if payload is not None and capacity>len(payload)+prefix:c['expected_memory']=[(OUTPUT,bytes([0xa5])*prefix+payload)]
                cases.append(c)
    # Stored header checks, alignment, zero/full capacity, complement and
    # truncation. Initial written position may alias output (live reload).
    for initial in (0,1,3,7,8,9,15):
        start=(initial+7)//8
        for length in (0,1,3,31):
            payload=bytes(range(length));blob=bytes(start)+struct.pack('<HH',length,65535-length)+payload
            for size in (0,1,3,4,len(blob)-1,len(blob),len(blob)+4):
                for capacity in (0,length,length+1,length+32):
                    cases.append(case('stored',[BUFFER,OUTPUT,capacity,DATA,POS,WRITTEN,size],[(BUFFER,context()),(DATA,blob+bytes(64)),(POS,words(initial)),(WRITTEN,words(0)),(OUTPUT,bytes([0xa5])*128)]))
    for size in (0,1,2,8):
        for data in (b'\x78\x01\x07',b'\0\0\7',b'\x79\x18\x07',b'\x88\x1c\x07',b'\x78\x20\x07'):
            cases.append(case('zlib',[BUFFER,OUTPUT,256,DATA,size],[(BUFFER,context()),(DATA,data+bytes(64))]))
    for position in (0,7,8,16,32):
        for size in (2,3,4):cases.append(case('dynamic',[BUFFER,TREE,DIST,CODE,DATA,POS,size],[(BUFFER,context()),(TREE,words(NODES,15,288)),(DIST,words(NODES+0x1000,15,32)),(CODE,words(NODES+0x2000,7,19)),(POS,words(position)),(DATA,bytes(64))]))
    return cases

def main():
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--installed',action='store_true');args=parser.parse_args()
    started=time.monotonic();original=Fat16((ROOT.parent/'IceStarOS_x86_32M.vhd').read_bytes()).read_kernel();assert hashlib.sha256(original).hexdigest()==ORIGINAL_SHA256
    variants,sizes=compile_variants(original,args.installed,'inflate',SLOTS,source='kernel/inflate.c',bindings={'kernel_fill_bytes':'0x815b02'})
    machines=[Machine(*s,original,b'') for s in [('original',original,BASE,[s for _,_,s,_ in SLOTS]),*variants]]
    counts={n:0 for n,_,_,_ in SLOTS};cases=scenarios()
    for i,c in enumerate(cases):
        if i%1000==0:print('DEFLATE',i,'/',len(cases),flush=True)
        try:
            expected=machines[0].execute(c)
            for m in machines[1:]:
                actual=m.execute(c);assert actual==expected,(m.label,expected,actual)
        except Exception:
            print('Failing inflate case',i,{k:v for k,v in c.items() if k!='setup'},flush=True);raise
        counts[SLOTS[c['function']][0]]+=1
    summary=dict(installed=args.installed,functions=len(SLOTS),scenarios=len(cases),executions=len(cases)*3,slots=sizes,scenarios_by_function=counts,seconds=round(time.monotonic()-started,3),
        checks=['RFC canonical Huffman tree oracles','bit shifts wrap at32','explicit dynamic header','zlib-independent decompression bytes',
        'stored/fixed/dynamic block paths','length/distance repeated copies','strict output limits and truncation','unaligned bit input','real nested helpers','all touched pages and cdecl state'],
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ('kernel/inflate.c','kernel/inflate.h','tests/kernel_inflate.py')})
    (BUILD/'inflate_summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
if __name__=='__main__':main()
