"""TrueType text with real glyph allocation, rasterization and pixel services."""
import argparse,json,random,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build';sys.path[:0]=[str(ROOT),str(BUILD/'test-deps')]
from tests.kernel_recovery_compare import compare
from tests.kernel_font_outline import fixture,words
from tests.kernel_font_heap import Machine as HeapMachine
from tests.kernel_font_bake import font_fixture,font_info
from tests.kernel_font_flatten import heap_setup
from tests.kernel_font_context import fbits
SLOTS=[['font_glyph_opaque', 'kernel_font_glyph_opaque_829003', 8556547, 8556851], ['font_glyph_clip', 'kernel_font_glyph_clip_829133', 8556851, 8557181], ['font_glyph_alpha', 'kernel_font_glyph_alpha_82927d', 8557181, 8557545], ['font_legacy_code', 'kernel_font_legacy_code_8293e9', 8557545, 8557701], ['font_string_legacy', 'kernel_font_string_legacy_829485', 8557701, 8558295], ['font_utf8_cont', 'kernel_font_utf8_cont_8296d7', 8558295, 8558343], ['font_utf8_decode', 'kernel_font_utf8_decode_829707', 8558343, 8558913], ['font_string_utf8', 'kernel_font_string_utf8_829941', 8558913, 8559437], ['font_string_lines', 'kernel_font_string_lines_829b4d', 8559437, 8560083], ['font_string_legacy_clip', 'kernel_font_string_legacy_clip_829dd3', 8560083, 8560703], ['font_string_legacy_alpha', 'kernel_font_string_legacy_alpha_82a03f', 8560703, 8561349], ['font_text_init', 'kernel_font_text_init_82a2c5', 8561349, 8561451]]
DATA,INFO,TEXT,OUT,TABLE,SURFACE,PIXELS,MAP=0x600000,0x640000,0x650000,0x660000,0x670000,0x680000,0x690000,0x700000
class Machine(HeapMachine):
    returns={3,5,6,8,11};aggregates=set();mutable_caller=set()
def base(depth=32,limit=0x2100000):
    raw=font_fixture();surface=bytearray(64);struct.pack_into('<I',surface,0,PIXELS);struct.pack_into('<II',surface,12,64,32);struct.pack_into('<II',surface,36,depth,0xff00ff)
    service=bytearray(80);struct.pack_into('<I',service,0,0x81b2f4 if depth==32 else 0x81b26e);struct.pack_into('<I',service,8,0x81bd26 if depth==32 else 0x81ba8a);struct.pack_into('<I',service,68,0x817322);struct.pack_into('<I',service,76,0x81b486 if depth==32 else 0x81b406)
    return [*heap_setup(limit),(DATA,raw),(INFO,font_info(raw)),(0x500000+8,words(SURFACE)),(SURFACE,bytes(surface)),(PIXELS,b'\x5a'*64*32*4),(0x1b0e4,words(TABLE)),(TABLE,bytes(service)),(OUT,bytes(32))]
def decode_oracle(raw):
    first=raw[0];n=1 if first<128 else 2 if first&0xe0==0xc0 else 3 if first&0xf0==0xe0 else 4 if first&0xf8==0xf0 else 0
    if not n:return -1,0
    if any(x&0xc0!=0x80 for x in raw[1:n]):return -1,0
    cp=first if n==1 else first&((1<<(7-n))-1)
    for b in raw[1:n]:cp=cp*64+(b&63)
    return (cp if (n==1 or cp>=[0,0,128,2048,65536][n]) and cp<=0x10ffff else -1),n

def scenarios():
    cases=[];rng=random.Random(0x829003)
    for value in range(1024):cases.append(fixture(5,[value],expected=int(value&0xc0==0x80)))
    raw_cases=[bytes([first,a,b,c]) for first in range(256) for a,b,c in [(0,0,0),(0x80,0x80,0x80),(0xbf,0xbf,0xbf),(0x9f,0x98,0x80)]]
    raw_cases += [chr(cp).encode()+bytes(4) for cp in [0,1,127,128,2047,2048,0xd7ff,0xe000,65535,65536,0x10ffff]]
    for raw in raw_cases:
        expected,n=decode_oracle(raw)
        cases.append(fixture(6,[TEXT,OUT],[(TEXT,raw),(OUT,b'\xa5'*4)],expected,[(OUT,words(n))]))
    for args in ([0,OUT],[TEXT,0],[0,0]):cases.append(fixture(6,args,expected=-1))
    for raw in raw_cases[::21]:
        for delta in range(4):cases.append(fixture(6,[TEXT,TEXT+delta],[(TEXT,raw+bytes(16))]))
    for code in [0,1,-1,0xa1a0,0xa1a1,0xa1fe,0xa9fe,0xa9ff,0xb0a0,0xb0a1,0xf7fe,0xf7ff,0x7fffffff,0x80000000]+[rng.randrange(0x10000) for _ in range(160)]:
        offset=None
        if 0xa1a0<code<=0xa9fe:offset=((code-0xa100>>8)*94+(code&0x5e))*12+8
        elif 0xb0a0<code<=0xf7fe:offset=((code-0xb000>>8)*94+(code&255))*12+0x2024
        setup=[(0x1b1a8,words(MAP))];result=0
        if offset is not None:result=code^0x12345678;setup.append((MAP+offset,words(result)))
        cases.append(fixture(3,[code],setup,result))
    # Each bitmap entry executes real font/heap/blend/write calls, including
    # width/height output aliases and supported 8/32bpp dispatch.
    for fn in (0,1,2):
        for depth in (8,32):
            for cp in (64,65,66):
                for xy in ((4,8),(0,0),(-2,3),(60,30)):
                    args=[INFO,fbits(16),cp,2,*xy]
                    if fn==1:args += [7,7]
                    args += [0xe15397]
                    if fn!=2:args += [0x173759]
                    args += [OUT,OUT+4,OUT+8,OUT+12]
                    cases.append(fixture(fn,args,base(depth)))
                for delta in (0,4,8,12):
                    args=[INFO,fbits(16),cp,2,4,8]+([64,32] if fn==1 else [])+[0xffffff]+([] if fn==2 else [0])+[OUT+delta]*4
                    cases.append(fixture(fn,args,base(depth)))
    # Strings: nonempty paths, upper clips, UTF8 skipping, space and newline,
    # wrapping and deliberately leaked bitmap on multiline lower boundary.
    for fn in (4,7,8,9,10):
        texts=[b'',b'A',b'AB',b'A B',b'ABA',b'A  BA']
        if fn in (7,8):texts += [b'\xffAB',b'A\xc0\x80B',b'A\xe4\xb8\xadB',b'A\rB\nA',b'\n\rAB']
        for raw in texts:
            for depth in (8,32):
                for gap in (0,1,3):
                    args=[INFO,fbits(16),TEXT,2,4,8]
                    if fn in (8,9):args += [12,24]
                    args += [0xe15397]
                    if fn!=10:args += [0x173759]
                    args += [gap]
                    if fn==8:args += [8,len(raw)]
                    cases.append(fixture(fn,args,[*base(depth),(TEXT,raw+b'\0')]))
        if fn==8:
            for x,y,maxx,maxy,n in [(4,8,64,32,99),(4,8,8,32,99),(4,8,64,4,99),(4,8,64,32,0),(4,8,64,32,1),(4,8,64,32,-1)]:
                cases.append(fixture(fn,[INFO,fbits(16),TEXT,2,x,y,maxx,maxy,0xffffff,0,1,8,n],[*base(),(TEXT,b'ABA\0')] ))
    # Nonempty original double-byte strings execute the real legacy table.
    for fn in (4,9,10):
        for pair in (b'\xa1\xa1',b'\xb0\xa1'):
            code=int.from_bytes(pair,'big')
            offset=(((code-0xa100)>>8)*94+(code&0x5e))*12+8 if code<0xb000 else (((code-0xb000)>>8)*94+(code&255))*12+0x2024
            args=[INFO,fbits(16),TEXT,2,4,8]+([7,5] if fn==9 else [])+[0xe15397]+([] if fn==10 else [0x173759])+[1]
            cases.append(fixture(fn,args,[*base(),(0x1b1a8,words(MAP)),(MAP+offset,words(65)),(TEXT,pair+b'A\0')]))
    # Transparent legacy text samples the desktop surface when colorkey hit.
    fg,bg=0xe15397,0x112233
    setup=base()+[(0x500000+12,words(SURFACE)),(SURFACE+40,words(0x5a5a5a5a)),(0x500000+8,words(SURFACE+128))]
    desktop=bytearray(64);struct.pack_into('<I',desktop,0,PIXELS+0x3000);struct.pack_into('<II',desktop,12,64,32)
    setup += [(SURFACE+128,bytes(desktop)),(PIXELS+0x3000,words(bg)*(64*32)),(TEXT,b'A\0')]
    pixels=bytearray(b'\x5a'*8192);color=sum(((((fg>>shift)&255)*255+((bg>>shift)&255))//256)<<shift for shift in (0,8,16))
    for y in range(4,8):
        for x in range(5,11):struct.pack_into('<I',pixels,(y*64+x)*4,color)
    cases.append(fixture(10,[INFO,fbits(16),TEXT,3,4,8,fg,1],setup,memory=[(PIXELS,bytes(pixels))]))
    # Independent rectangle coverage and channel blend oracle (glyph A).
    for depth in (8,32):
        pixels=bytearray(b'\x5a'*8192);fg,bg=0xe15397,0x173759
        color=sum(((((fg>>shift)&255)*255+((bg>>shift)&255))//256)<<shift for shift in (0,8,16)) if depth==32 else fg&255
        for y in range(4,8):
            for x in range(5,11):
                off=(y*64+x)*(4 if depth==32 else 1)
                pixels[off:off+(4 if depth==32 else 1)]=words(color) if depth==32 else bytes([color])
        cases.append(fixture(0,[INFO,fbits(16),65,2,4,8,fg,bg,OUT,OUT+4,OUT+8,OUT+12],base(depth),memory=[(PIXELS,bytes(pixels)),(OUT,words(6,4,1,-4))]))
    for data in (0,DATA):cases.append(fixture(11,[data,INFO],base(),int(bool(data))))
    return cases

def main():
    p=argparse.ArgumentParser(__doc__);p.add_argument('--installed',action='store_true');args=p.parse_args()
    compare('font_text',SLOTS,Machine,scenarios(),args.installed,['all UTF8 lead bytes and malformed/overlong sequences','legacy mapping formula','real glyph/heap/raster/pixel calls','nonempty opaque/clipped/alpha text and multiline wrap','all touched pages, output aliases and cdecl/x87 state'])
if __name__=='__main__':main()
