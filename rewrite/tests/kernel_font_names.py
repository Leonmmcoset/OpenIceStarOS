"""Unicode and SFNT naming recovery with real binary and table-level oracles."""
import argparse
from pathlib import Path
import random
import struct
import sys
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build';sys.path[:0]=[str(ROOT),str(BUILD/'test-deps')]
from tests.kernel_recovery_compare import compare
from tests.kernel_font_outline import fixture,words
from tests.kernel_font_heap import Machine as HeapMachine
from tests.kernel_font_init import sfnt
from tools.kernel_image import Fat16
DATA,TEXT,UTF,BUFFER,OUTPUT=0x600000,0x610000,0x620000,0x500000,0x510000
SLOTS=[
    ('font_name_prefix','kernel_font_name_prefix_8365b2',0x8365b2,0x83689a),
    ('font_name_equal','kernel_font_name_equal_83689a',0x83689a,0x8368d8),
    ('font_name_string','kernel_font_name_string_8368d8',0x8368d8,0x836a34),
    ('font_name_pair','kernel_font_name_pair_836a34',0x836a34,0x836cc0),
    ('font_match','kernel_font_match_836cc0',0x836cc0,0x836f08),
    ('font_find_name','kernel_font_find_name_836f08',0x836f08,0x836f70),
    ('font_find_name_api','kernel_font_find_name_api_83700c',0x83700c,0x83702e),
    ('font_name_equal_api','kernel_font_name_equal_api_83702e',0x83702e,0x837058),
]
class Machine(HeapMachine):
    returns=set(range(8));mutable_caller={0};aggregates=set()
def name_table(records):
    data=b'';out=b'\0\0'+struct.pack('>HH',len(records),6+len(records)*12)
    for platform,encoding,language,name,text in records:
        raw=text if isinstance(text,bytes) else text.encode('utf-16-be')
        out+=struct.pack('>6H',platform,encoding,language,name,len(raw),len(data));data+=raw
    return out+data

def scenarios():
    cases=[];rng=random.Random(0x8365b2)
    examples=['','A','OpenIceStarOS','中文字体','café','αβγ','\u07ff\u0800','😀🚀','\U0010ffff','A\0B']
    examples+=[''.join(chr(rng.choice([rng.randrange(0xd800),rng.randrange(0xe000,0x10000),rng.randrange(0x10000,0x110000)])) for _ in range(rng.randrange(1,7))) for i in range(80)]
    for text in examples:
        utf=text.encode('utf-16-be');raw=text.encode();n=len(raw)
        for size in sorted(set([0,n,max(0,n-1),n+1])):
            for fn in (0,1,7):
                expected=n if fn==0 else 1
                if size<n:expected=-1 if fn==0 else 0
                elif size>n and fn!=0:expected=0
                cases.append(fixture(fn,[TEXT,size,UTF,len(utf)],[(TEXT,raw+b'\0'),(UTF,utf+bytes(8))],expected))
        if raw:
            for i in sorted(set([0,n//2,n-1])):
                changed=bytearray(raw);changed[i]^=1
                for fn in (0,1,7):cases.append(fixture(fn,[TEXT,n,UTF,len(utf)],[(TEXT,bytes(changed)),(UTF,utf+bytes(8))],-1 if fn==0 else 0))
    for raw in (b'\xdc\0',b'\xdf\xff',b'\xd8\0\0\0',b'\xdb\xff\xff\xff'):
        for length in (0,1,2,3,4,5,-1):cases.append(fixture(0,[TEXT,length,UTF,len(raw)],[(TEXT,b'\xf0\x80\x80\x80'+bytes(16)),(UTF,raw+bytes(8))]))
    records=[(0,3,0,16,'OpenIce'),(0,3,0,17,'Regular'),(3,1,1033,1,'OpenIce'),(3,1,1033,2,'Bold'),(3,10,2052,3,'中文😀'),(1,0,0,1,'Unsupported')]
    table=name_table(records);head=bytearray(64)
    raw,offsets=sfnt({b'name':table,b'head':bytes(head)})
    base=[(DATA,raw),(BUFFER,words(0,DATA,0)),(TEXT,b'OpenIce Bold\0')]
    for i,(p,e,l,n,t) in enumerate(records):
        rawname=t.encode('utf-16-be');offset=offsets[b'name']+6+len(records)*12+sum(len(x[4].encode('utf-16-be')) for x in records[:i])
        for output in (OUTPUT,BUFFER+4,BUFFER+8):
            cases.append(fixture(2,[BUFFER,output,p,e,l,n],base,DATA+offset,[(output,words(len(rawname)))]))
        cases.append(fixture(2,[BUFFER,OUTPUT,p,e,l,n+100],base,0,[(OUTPUT,b'\xa5'*4)]))
    for text in ('OpenIce','OpenIce Regular','OpenIce Bold','OpenIce Wrong','中文😀','Unsupported','', 'OpenIce  Bold'):
        for first,second in ((16,17),(1,2),(3,-1),(16,-1)):
            cases.append(fixture(3,[DATA,offsets[b'name'],TEXT,len(text.encode()),first,second],[(DATA,raw),(TEXT,text.encode()+b'\0')]))
        for flags in (0,1,2,3,4,7,8,0xffffffff):
            for style in (0,1,2,7):
                font=bytearray(raw);struct.pack_into('>H',font,offsets[b'head']+44,style)
                for fn,args in ((4,[DATA,0,TEXT,flags]),(5,[DATA,TEXT,flags]),(6,[DATA,TEXT,flags])):
                    cases.append(fixture(fn,args,[(DATA,bytes(font)),(TEXT,text.encode()+b'\0')]))
    # Pair matching accepts only an adjacent record with identical platform,
    # encoding and language; empty suffix and fallback nameID3 are covered.
    for p in (0,1,3):
        for enc in (0,1,10):
            for suffix in ('','Regular','中文'):
                records=[(p,enc,1033,16,'Family'),(p,enc,1033,17,suffix),(3,1,0,3,'Fallback')]
                raw,off=sfnt({b'name':name_table(records),b'head':bytes(head)})
                for text in ('Family','Family '+suffix,'Fallback','Family Wrong'):
                    for second in (17,0xffffffff):cases.append(fixture(3,[DATA,off[b'name'],TEXT,len(text.encode()),16,second],[(DATA,raw),(TEXT,text.encode()+b'\0')]))
    # Missing tables and invalid signatures: no implementation substitutions.
    for raw in (bytes(128),sfnt({b'head':bytes(head)})[0]):
        for fn,args in ((2,[BUFFER,OUTPUT,3,1,0,1]),(4,[DATA,0,TEXT,0]),(5,[DATA,TEXT,0]),(6,[DATA,TEXT,0])):
            cases.append(fixture(fn,args,[(DATA,raw),(BUFFER,words(0,DATA,0)),(TEXT,b'Any\0')],0 if fn in (2,4) else -1))
    # Real shipped font: independent directory/name-record parser.
    font=Fat16((ROOT.parent/'IceStarOS_x86_32M.vhd').read_bytes()).read_file(b'ICESTAR    ',b'FONT       ',b'FIRA    TTF')
    tables={font[12+i*16:16+i*16]:struct.unpack_from('>II',font,20+i*16) for i in range(struct.unpack_from('>H',font,4)[0])}
    names=tables[b'name'][0];count,start=struct.unpack_from('>HH',font,names+2)
    for i in range(count):
        p,e,l,n,size,off=struct.unpack_from('>6H',font,names+6+i*12)
        cases.append(fixture(2,[BUFFER,OUTPUT,p,e,l,n],[(DATA,font),(BUFFER,words(0,DATA,0))],DATA+names+start+off,[(OUTPUT,words(size))]))
        if n==3 and (p==0 or p==3 and e in (1,10)):
            text=font[names+start+off:names+start+off+size].decode('utf-16-be').encode()+b'\0'
            for fn,args in ((4,[DATA,0,TEXT,0]),(5,[DATA,TEXT,0]),(6,[DATA,TEXT,0])):
                cases.append(fixture(fn,args,[(DATA,font),(TEXT,text)],1 if fn==4 else 0))
    # TTC: miss first face, match the second; absolute table offsets retained.
    raw,_=sfnt({b'name':name_table([(3,1,1033,3,'Collection Face')]),b'head':bytes(head)})
    collection=bytearray(raw);collection[:20]=b'ttcf'+struct.pack('>II2I',0x10000,2,0x100,0x200)
    collection[0x100:0x10c]=struct.pack('>IHHHH',0x10000,0,0,0,0)
    collection[0x200:0x22c]=raw[:44]
    for text,offset in ((b'Collection Face\0',0x200),(b'Missing\0',-1)):
        for fn in (5,6):cases.append(fixture(fn,[DATA,TEXT,0],[(DATA,bytes(collection)),(TEXT,text)],offset))
    return cases

def main():
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--installed',action='store_true');args=parser.parse_args()
    compare('font_names',SLOTS,Machine,scenarios(),args.installed,['UTF8 vs UTF16BE independent encodings','surrogate pairs and malformed units','caller argument mutation','name record pointers and lengths','adjacent family/subfamily selection','style matching and collection search','all touched pages and cdecl state'])
if __name__=='__main__':main()
