"""Real PNG x86/C parity, with generated PNG, zlib and independent pixel oracles."""
import argparse
import copy
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
from tests.kernel_font_flatten import heap_setup
BASE,DATA,BUFFER,OUTPUT,HEAP=0x800000,0x600000,0x500000,0x510000,0x2000000
SLOTS=[
    ('png_paeth', 'kernel_png_paeth_8393a1', 8623009, 8623193),
    ('png_scanline', 'kernel_png_scanline_839459', 8623193, 8624093),
    ('png_unfilter', 'kernel_png_unfilter_8397dd', 8624093, 8624275),
    ('png_unpad', 'kernel_png_unpad_839893', 8624275, 8624529),
    ('png_postprocess', 'kernel_png_postprocess_839991', 8624529, 8624799),
    ('png_format', 'kernel_png_format_839a9f', 8624799, 8625129),
    ('png_release_input', 'kernel_png_release_input_839be9', 8625129, 8625187),
    ('png_header', 'kernel_png_header_839c23', 8625187, 8625933),
    ('png_decode', 'kernel_png_decode_839f0d', 8625933, 8627401),
    ('png_create', 'kernel_png_create_83a4c9', 8627401, 8627577),
    ('png_from_memory', 'kernel_png_from_memory_83a579', 8627577, 8627643),
    ('png_destroy', 'kernel_png_destroy_83a5bb', 8627643, 8627697),
    ('png_error', 'kernel_png_error_83a5f1', 8627697, 8627709),
    ('png_error_line', 'kernel_png_error_line_83a5fd', 8627709, 8627721),
    ('png_width', 'kernel_png_width_83a609', 8627721, 8627731),
    ('png_height', 'kernel_png_height_83a613', 8627731, 8627743),
    ('png_bpp', 'kernel_png_bpp_83a61f', 8627743, 8627785),
    ('png_channels', 'kernel_png_channels_83a649', 8627785, 8627875),
    ('png_depth', 'kernel_png_depth_83a6a3', 8627875, 8627887),
    ('png_padded_bpp', 'kernel_png_padded_bpp_83a6af', 8627887, 8627947),
    ('png_pixel_format', 'kernel_png_pixel_format_83a6eb', 8627947, 8627959),
    ('png_pixels', 'kernel_png_pixels_83a6f7', 8627959, 8627971),
    ('png_size', 'kernel_png_size_83a703', 8627971, 8627983),
]
INDEX={n.removeprefix('png_'):i for i,(n,_,_,_) in enumerate(SLOTS)}
class Machine(HeapMachine):
    returns={i for i,(n,_,_,_) in enumerate(SLOTS) if n.removeprefix('png_') not in
        {'scanline','unfilter','unpad','postprocess','release_input','destroy'}}
    mutable_caller=set()
def case(name,args,setup=(),expected=None,memory=()):
    return fixture(INDEX[name],args,setup,expected,memory)
def context(w=3,h=2,color=6,depth=8,pixels=0,size=0,error=0,line=0,state=2,source=DATA,length=0,owned=0):
    fmt={(0,1):5,(0,2):6,(0,4):7,(0,8):8,(2,8):1,(2,16):2,(4,1):9,(4,2):10,(4,4):11,(4,8):12,(6,8):3,(6,16):4}.get((color,depth),0)
    return words(w,h,color,depth,fmt,pixels,size,error,line,state,source,length)+bytes([owned,0,0,0])
def chunk(tag,body):
    return struct.pack('>I',len(body))+tag+body+struct.pack('>I',zlib.crc32(tag+body))
def png(w,h,color,depth,raw,level=0,strategy=zlib.Z_DEFAULT_STRATEGY,split=False,extra=b''):
    obj=zlib.compressobj(level,zlib.DEFLATED,15,8,strategy);compressed=obj.compress(raw)+obj.flush()
    parts=[compressed[:3],compressed[3:]] if split else [compressed]
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,depth,color,0,0,0))+extra+b''.join(chunk(b'IDAT',p) for p in parts)+chunk(b'IEND',b'')
def paeth(a,b,c):
    p=a+b-c;d=[abs(p-a),abs(p-b),abs(p-c)];return (a,b,c)[d.index(min(d))]
def scan_model(out,source,previous,bw,f,n):
    out=bytearray(out);source=bytes(source)
    if f>4:return out
    limit=max(bw,n) if f in (1,3,4) else n
    for i in range(limit):
        a=out[i-bw] if i>=bw else 0;b=previous[i] if previous else 0;c=previous[i-bw] if previous and i>=bw else 0
        pred=(0,a,b,(a+b)//2,paeth(a,b,c))[f]
        out[i]=(source[i]+pred)&255
    return out

def scenarios():
    rng=random.Random(0x8393a1);cases=[]
    for i in range(300):
        a,b,c=[rng.randrange(256) for _ in range(3)]
        cases.append(case('paeth',[a,b,c],expected=paeth(a,b,c)))
    for f in (0,1,2,3,4,5,255,256,260):
        for bw in (0,1,2,3,4,8):
            for n in (0,1,3,8,31):
                for prev in (0,DATA+512):
                    source=bytes(rng.randrange(256) for _ in range(64));previous=bytes(rng.randrange(256) for _ in range(64))
                    out=bytes([0xa5])*64;expected=scan_model(out,source,previous if prev else None,bw,f&255,n)
                    ctx=bytearray(context());
                    if f&255>4:struct.pack_into('<II',ctx,28,4,0x2aa)
                    cases.append(case('scanline',[BUFFER,OUTPUT,DATA,prev,bw,f,n],[(BUFFER,context()),(DATA,source),(DATA+512,previous),(OUTPUT,out)],
                        memory=[(OUTPUT,bytes(expected)),(BUFFER,bytes(ctx))]))
                    if n>=bw and f<5:
                        for delta in (0,1,-1):
                            cases.append(case('scanline',[BUFFER,DATA+delta,DATA,prev,bw,f,n],[(BUFFER,context()),(DATA-1,b'\xa5'+source+b'\xa5'),(DATA+512,previous)]))
    for linebits in (0,1,2,7,8,9,13,16,23):
        for h in (0,1,2,7):
            padded=(linebits+7)&~7;source=bytes(rng.randrange(256) for _ in range(max(1,padded//8*h)))
            out=bytearray([0xa5])*64;bits=''.join(f'{b:08b}' for b in source)
            joined=''.join(bits[y*padded:y*padded+linebits] for y in range(h))
            for j,b in enumerate(joined):
                mask=1<<(7-(j&7));out[j//8]=(out[j//8]&~mask)|(int(b)*mask)
            cases.append(case('unpad',[OUTPUT,DATA,linebits,padded,h],[(DATA,source),(OUTPUT,bytes([0xa5])*64)],memory=[(OUTPUT,bytes(out))]))
            cases.append(case('unpad',[DATA,DATA,linebits,padded,h],[(DATA,source+bytes(64))]))
    for color in (0,2,3,4,6,1,7,255,0xffffffff):
        for depth in (0,1,2,4,8,16,255,0xffffffff):
            ctx=context(color=color,depth=depth);channels={0:1,2:3,4:2,6:4}.get(color,0);bpp=channels*depth&0xffffffff
            for name,result in [('format',struct.unpack_from('<I',ctx,16)[0]),('channels',channels),('depth',depth),('bpp',bpp),('padded_bpp',(bpp+(bpp&7))&0xffffffff)]:
                cases.append(case(name,[BUFFER],[(BUFFER,ctx)],result))
    for name,off in [('error',28),('error_line',32),('width',0),('height',4),('pixel_format',16),('pixels',20),('size',24)]:
        for val in (0,1,0x7fffffff,0x80000000,0xffffffff):
            ctx=bytearray(context());struct.pack_into('<I',ctx,off,val)
            cases.append(case(name,[BUFFER],[(BUFFER,bytes(ctx))],val))
    valid=png(3,2,6,8,bytes(26))
    for n in (0,1,8,16,28,29,32,33,len(valid)):
        cases.append(case('header',[BUFFER],[(BUFFER,context(length=n)),(DATA,valid)]))
    for i in list(range(8))+list(range(12,29)):
        mutated=bytearray(valid);mutated[i]^=1
        cases.append(case('header',[BUFFER],[(BUFFER,context(length=len(valid))),(DATA,bytes(mutated))]))
    for state in (0,1,2,3,0xffffffff):
        for err in (0,1,7):
            cases.append(case('header',[BUFFER],[(BUFFER,context(state=state,error=err,length=len(valid))),(DATA,valid)],err))
            cases.append(case('decode',[BUFFER],[*heap_setup(),(BUFFER,context(state=state,error=err,length=len(valid))),(DATA,valid)]))
    for limit in (HEAP,HEAP+1024,HEAP+2048,HEAP+4096,HEAP+0x100000):
        cases.append(case('create',[],heap_setup(limit),HEAP+16 if limit>HEAP+1024 else 0))
        cases.append(case('from_memory',[DATA,31],heap_setup(limit),HEAP+16 if limit>HEAP+1024 else 0))
    for owned in (0,1,255):
        for source in (0,HEAP+16):
            cases.append(case('release_input',[BUFFER],[*heap_setup(),(HEAP,words(HEAP+1024,0,1,23)),(HEAP+1024,words(0,HEAP,0,0)),(BUFFER,context(source=source,length=100,owned=owned))]))
    for have_pixels in (0,1):
        for owned in (0,1):
            setup=[*heap_setup(),(HEAP,words(HEAP+1024,0,1,23)),(HEAP+1024,words(HEAP+2048,HEAP,1,23)),(HEAP+2048,words(HEAP+3072,HEAP+1024,1,23)),(HEAP+3072,words(0,HEAP+2048,0,0)),
                (HEAP+16,context(pixels=HEAP+1040 if have_pixels else 0,source=HEAP+2064,owned=owned))]
            cases.append(case('destroy',[HEAP+16],setup))
    # PNG spec-derived pixels, encoded with all five row filters. Low-depth
    # output tail bits intentionally preserve the allocator's original bytes.
    for color,depth in ((0,1),(0,2),(0,4),(0,8),(2,8),(2,16),(4,1),(4,2),(4,4),(4,8),(6,8),(6,16)):
        bpp={0:1,2:3,4:2,6:4}[color]*depth
        for w,h in ((1,1),(3,2),(8,5),(9,3)):
            n=(w*bpp+7)//8;bw=(bpp+7)//8;rows=[bytes(rng.randrange(256) for _ in range(n)) for _ in range(h)]
            for f in range(5):
                encoded=[]
                for y,row in enumerate(rows):
                    prev=rows[y-1] if y else bytes(n);line=[]
                    for i,v in enumerate(row):
                        a=row[i-bw] if i>=bw else 0;b=prev[i];c=prev[i-bw] if i>=bw else 0
                        line.append((v-(0,a,b,(a+b)//2,paeth(a,b,c))[f])&255)
                    encoded.append(bytes([f])+bytes(line))
                raw=b''.join(encoded);pixelbits=''.join(''.join(f'{v:08b}' for v in row)[:w*bpp] for row in rows)
                output=bytes(int(pixelbits[i:i+8].ljust(8,'0'),2) for i in range(0,len(pixelbits),8))
                for level,strategy in ((0,0),(6,zlib.Z_FIXED),(6,0)):
                    data=png(w,h,color,depth,raw,level,strategy,split=True,extra=chunk(b'tEXt',b'comment\0generated fixture'))
                    for name in ('postprocess','decode'):
                        if name=='postprocess':
                            # Initial output contains a5, preserving final unused bits.
                            expected=bytearray(output)
                            if len(pixelbits)&7:expected[-1]|=0xa5&((1<<(8-(len(pixelbits)&7)))-1)
                            c=case(name,[BUFFER,OUTPUT,DATA,BUFFER],[(BUFFER,context(w,h,color,depth)),(OUTPUT,bytes([0xa5])*len(output)),(DATA,raw)],memory=[(OUTPUT,bytes(expected))])
                        else:
                            # decode writes from zeroed, real heap allocations.
                            c=case(name,[BUFFER],[*heap_setup(),(BUFFER,context(w,h,color,depth,length=len(data))),(DATA,data)])
                            c['decode_pixels']=output;c['decode_ctx']=BUFFER;c['pixelbits']=len(pixelbits)
                        cases.append(c)
    for tag in (b'ABCD',b'abcd',b'PLTE',b'IEND'):
        data=png(3,2,6,8,bytes(26),extra=chunk(tag,b'xyz'))
        cases.append(case('decode',[BUFFER],[*heap_setup(),(BUFFER,context(length=len(data))),(DATA,data)]))
    for limit in (HEAP,HEAP+1024,HEAP+2048,HEAP+3072,HEAP+4096):
        cases.append(case('decode',[BUFFER],[*heap_setup(limit),(BUFFER,context(length=len(valid))),(DATA,valid)]))
    # Direct row orchestrator, including invalid filter, compact in-place and
    # padded input, checks all bytes against the original, without mocks.
    for f in range(7):
        for bpp in (1,2,4,8,24,32,48,64):
            for h in (0,1,4):
                w=9;n=(w*bpp+7)//8;raw=b''.join(bytes([f])+bytes(rng.randrange(256) for _ in range(n)) for y in range(h))
                for dest in (OUTPUT,DATA):cases.append(case('unfilter',[BUFFER,dest,DATA,w,h,bpp],[(BUFFER,context()),(DATA,raw+bytes(64)),(OUTPUT,bytes([0xa5])*512)]))
    return cases

def main():
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--installed',action='store_true');args=parser.parse_args()
    started=time.monotonic();original=Fat16((ROOT.parent/'IceStarOS_x86_32M.vhd').read_bytes()).read_kernel()
    assert hashlib.sha256(original).hexdigest()==ORIGINAL_SHA256
    variants,sizes=compile_variants(original,args.installed,'png',SLOTS,source='kernel/png.c',bindings={'kernel_allocate':'0x815464','kernel_free_allocation':'0x815978','kernel_move_bytes':'0x815a90'})
    machines=[Machine(*s,original,b'') for s in [('original',original,BASE,[s for _,_,s,_ in SLOTS]),*variants]]
    counts={n:0 for n,_,_,_ in SLOTS};cases=scenarios()
    for i,c in enumerate(cases):
        if i%1000==0:print('PNG',i,'/',len(cases),flush=True)
        try:
            expected=machines[0].execute(c)
            for m in machines:
                actual=expected if m is machines[0] else m.execute(c)
                assert actual==expected,(m.label,expected,actual)
                if 'decode_pixels' in c:
                    ctx=bytes(m.uc.mem_read(c['decode_ctx'],52));ptr,size=struct.unpack_from('<II',ctx,20)
                    err,line=struct.unpack_from('<II',ctx,28)
                    if err:
                        assert err==4 and line in (0x208,0x1da),(err,line)
                        continue
                    assert size==len(c['decode_pixels'])
                    pixels=bytes(m.uc.mem_read(ptr,size));oracle=c['decode_pixels']
                    if c['pixelbits']&7:
                        mask=0xff<<(8-(c['pixelbits']&7))&255
                        pixels=pixels[:-1]+bytes([pixels[-1]&mask]);oracle=oracle[:-1]+bytes([oracle[-1]&mask])
                    assert pixels==oracle,(m.label,pixels,oracle)
        except Exception:
            print('Failing PNG case',i,{k:v for k,v in c.items() if k!='setup'},flush=True);raise
        counts[SLOTS[c['function']][0]]+=1
    summary=dict(installed=args.installed,functions=len(SLOTS),scenarios=len(cases),executions=len(cases)*3,
        slots=sizes,scenarios_by_function=counts,seconds=round(time.monotonic()-started,3),
        checks=['all five PNG row filters','12 original color/depth mappings','sub-byte row compaction',
        'generated PNG/zlib stored/fixed/dynamic streams','spec-derived pixel bytes','real original/installed heap',
        'header/chunk/state/errors','allocation failures','overlapping buffers','all touched data and heap pages','cdecl registers and stack'],
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ('kernel/png.c','kernel/png.h','tests/kernel_png.py','tests/kernel_font_outline.py')})
    (BUILD/'png_summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
if __name__=='__main__':main()
