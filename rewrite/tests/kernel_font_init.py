"""SFNT initializer: real original helpers, partial state, TrueType and CFF."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import sys
import time
ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(BUILD/'test-deps')]
from tools.kernel_image import Fat16,ORIGINAL_SHA256
from tests.kernel_x_loader import FLAGS,run
from tests.kernel_font_cmap import Machine,words
BASE,DATA,BUFFER,FONT,MASK=0x800000,0x600000,0x500000,0xa00000,0xffffffff
ENTRY,END=0x82c3bc,0x82ca7c


def compile_variants(original,installed):
    obj=BUILD/'kernel_font_init_preview.o'
    run(['gcc',*FLAGS,'-c','kernel/font_init.c','-o',str(obj)])
    undefined=run(['nm','-u',str(obj)],capture_output=True,text=True).stdout.splitlines()
    provides=''.join(f'PROVIDE({s} = 0x{s.rsplit("_",1)[1]});\n' for s in (line.split()[-1] for line in undefined))
    outputs=[];sizes={}
    for fixed in (False,True):
        label='fixed' if fixed else 'ordinary';script,elf=BUILD/f'font_init_{label}.ld',BUILD/f'font_init_{label}.elf'
        section=f'.font_init {ENTRY:#x} : {{ *(.text*) }}\nASSERT(SIZEOF(.font_init) <= {END-ENTRY}, "init overflow")' if fixed else '. = 0x300000; .text : { *(.text*) }'
        script.write_text('OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_font_init_82c3bc)\n'+provides+
            'SECTIONS {\n'+section+'\n/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n'
            '.unexpected : { *(*) }\nASSERT(SIZEOF(.unexpected)==0, "unexpected section")\n}\n')
        run(['ld','-m','elf_i386','-T',str(script),'-o',str(elf),str(obj)])
        body_path=BUILD/f'font_init_{label}.bin';run(['objcopy','-O','binary',str(elf),str(body_path)])
        body=body_path.read_bytes()
        if fixed:
            assert 0<len(body)<=END-ENTRY
            candidate=bytearray(original);candidate[ENTRY-BASE:END-BASE]=body+b'\x90'*(END-ENTRY-len(body))
            sizes=dict(compiled_bytes=len(body),slot_bytes=END-ENTRY)
            if installed:
                production=(BUILD/'openicestaros_kernel.x').read_bytes()
                assert production[ENTRY-BASE:END-BASE]==candidate[ENTRY-BASE:END-BASE]
                candidate=production
            outputs.append((label,bytes(candidate),BASE,[ENTRY]))
        else:outputs.append((label,body,0x300000,[0x300000]))
    return outputs,sizes


def cmap(records):
    raw=bytearray(struct.pack('>HH',0,len(records))+bytes(len(records)*8))
    for i,(platform,encoding,relative) in enumerate(records):
        struct.pack_into('>HHI',raw,4+i*8,platform,encoding,relative)
    return bytes(raw)


def sfnt(tables,offset=0):
    raw=bytearray(0x6000)
    struct.pack_into('>IHHHH',raw,offset,0x10000,len(tables),0,0,0)
    locations={}
    for i,(tag,body) in enumerate(tables.items()):
        location=0x1000+i*512;locations[tag]=location
        struct.pack_into('>4sIII',raw,offset+12+i*16,tag,0,location,len(body))
        raw[location:location+len(body)]=body
    return bytes(raw),locations


def integer(value):return bytes([29])+struct.pack('>I',value&MASK)
def index(items):
    if not items:return b'\0\0'
    offsets=[1]
    for item in items:offsets.append(offsets[-1]+len(item))
    return struct.pack('>HB',len(items),2)+b''.join(struct.pack('>H',o) for o in offsets)+b''.join(items)


def cff(charstrings=128,charstring_type=2,fdarray=0,fdselect=0,private=False):
    raw=bytearray(256);raw[:4]=bytes([1,0,4,4])
    dictionary=integer(charstrings)+bytes([17])+integer(charstring_type)+bytes([12,6])
    dictionary+=integer(fdarray)+bytes([12,36])+integer(fdselect)+bytes([12,37])
    if private:dictionary+=integer(6)+integer(180)+bytes([18])
    body=index([b'A'])+index([dictionary])+index([])+index([b'\x0b'])
    raw[4:4+len(body)]=body
    if charstrings:
        payload=index([b'\x0e']);raw[charstrings:charstrings+len(payload)]=payload
    if fdarray:
        payload=index([b'']);raw[fdarray:fdarray+len(payload)]=payload
    if private:
        raw[180:186]=integer(6)+bytes([19])
        local=index([b'\x0b']);raw[186:186+len(local)]=local
    if fdselect:raw[fdselect:fdselect+3]=b'\0\0\0'
    return bytes(raw)


def base_tables(records=((3,1,32),),fmt=1,glyphs=17):
    head=bytearray(64);struct.pack_into('>H',head,50,fmt)
    maxp=bytearray(6);struct.pack_into('>H',maxp,4,glyphs)
    return {b'cmap':cmap(records),b'loca':bytes(16),b'head':bytes(head),b'glyf':bytes(16),
            b'hhea':bytes(40),b'hmtx':bytes(16),b'maxp':bytes(maxp)}


def fixture(raw,locations,offset=0,data=DATA,initial=None,expected=None,selected=None):
    if initial is None:initial=bytes([0xa5])*160
    setup=[(BUFFER,initial),(data,raw)]
    memory=[(BUFFER+4,words(data,offset)),(BUFFER+56,bytes(12))]
    if expected==1 and b'CFF ' not in locations:
        memory += [(BUFFER+12,words(struct.unpack_from('>H',raw,locations[b'maxp']+4)[0] if b'maxp' in locations else 65535)),
            (BUFFER+44,words(MASK,selected if selected is not None else locations[b'cmap']+32)),
            (BUFFER+52,words(struct.unpack_from('>H',raw,locations[b'head']+50)[0]))]
    c=dict(function=0,args=[BUFFER,data,offset],setup=setup,expected_memory=memory)
    if expected is not None:c['expected_return']=expected
    return c


def scenarios(font):
    cases=[];rng=random.Random(0x82c3bc)
    essential=(b'cmap',b'head',b'hhea',b'hmtx',b'loca')
    for mask in range(32):
        tables=base_tables()
        for i,tag in enumerate(essential):
            if mask&(1<<i):del tables[tag]
        raw,locations=sfnt(tables)
        cases.append(fixture(raw,locations,expected=int(mask==0)))
    record_sets=[[],[(1,0,32)],[(3,0,32)],[(3,1,32)],[(3,10,64)],[(0,5,96)],
        [(3,10,64),(0,0,128),(3,1,192)],[(3,1,32),(1,0,128)],[(3,2,32),(3,10,0xfffff000)]]
    for records in record_sets:
        for offset in (0,16,128):
            for maximum in (False,True):
                for fmt in (0,1,2,65535):
                    tables=base_tables(records,fmt,65535 if maximum else 17)
                    if not maximum:del tables[b'maxp']
                    order=list(tables);rng.shuffle(order);tables={k:tables[k] for k in order}
                    raw,locations=sfnt(tables,offset)
                    chosen=None
                    for platform,encoding,relative in records:
                        if platform==0 or platform==3 and encoding in (1,10):chosen=(locations[b'cmap']+relative)&MASK
                    cases.append(fixture(raw,locations,offset,expected=int(bool(chosen)),selected=chosen))
    for charstrings in (0,128):
        for kind in (1,2,3):
            for fdarray,fdselect in ((0,0),(160,0),(160,200)):
                for private in (False,True):
                    tables=base_tables();del tables[b'glyf'];del tables[b'loca']
                    tables[b'CFF ']=cff(charstrings,kind,fdarray,fdselect,private)
                    raw,locations=sfnt(tables)
                    c=fixture(raw,locations,expected=int(kind==2 and charstrings!=0 and (not fdarray or fdselect!=0)))
                    c['expected_memory']=[(BUFFER+4,words(DATA,0)),(BUFFER+56,words(DATA+locations[b'CFF '],0,0x20000000))]
                    cases.append(c)
    # Early failures leave unrelated caller-owned fields and old caches intact.
    for prefix in (bytes(160),bytes([0x5a])*160,bytes(rng.getrandbits(8) for _ in range(160))):
        for missing in (b'head',b'cmap',b'hmtx'):
            tables=base_tables();del tables[missing];raw,locations=sfnt(tables)
            cases.append(fixture(raw,locations,initial=prefix,expected=0))
    # Aliasing the output with table bodies exercises partial write/read order.
    for delta in (0,4,16,48,256,4096):
        tables=base_tables();raw,locations=sfnt(tables)
        c=fixture(raw,locations)
        c['args'][0]=DATA+delta;c['setup']=c['setup'][1:];c['expected_memory']=[]
        cases.append(c)
    # Real original Fira initialization: all published table fields and cmap.
    from tests.kernel_font_glyph import real_info
    info,tables,count,fmt,selected=real_info(font)
    c=dict(function=0,args=[BUFFER,FONT,0],setup=[(BUFFER,bytes([0xa5])*160)],expected_return=1,
        expected_memory=[(BUFFER+4,info[4:44]),(BUFFER+44,words(MASK,selected,fmt)),(BUFFER+56,bytes(12))])
    cases.append(c)
    return cases


def main():
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--installed',action='store_true');args=parser.parse_args()
    started=time.monotonic();disk=Fat16((ROOT.parent/'IceStarOS_x86_32M.vhd').read_bytes());original=disk.read_kernel()
    assert hashlib.sha256(original).hexdigest()==ORIGINAL_SHA256
    font=disk.read_file(b'ICESTAR    ',b'FONT       ',b'FIRA    TTF')
    variants,sizes=compile_variants(original,args.installed)
    machines=[Machine(*spec,original,font) for spec in [('original',original,BASE,[ENTRY]),*variants]]
    cases=scenarios(font)
    for i,c in enumerate(cases):
        try:
            expected=machines[0].execute(c)
            for m in machines[1:]:assert m.execute(c)==expected,(m.label,expected)
        except Exception:
            print('Failing initializer case',i,{k:v for k,v in c.items() if k!='setup'},flush=True);raise
    summary=dict(installed=args.installed,scenarios=len(cases),executions=3*len(cases),slot=sizes,
        boundary_scope='All original/recovered CFF and SFNT helpers execute without interception',
        seconds=round(time.monotonic()-started,3),
        checks=['partial output state','TrueType required/optional tables','Unicode cmap selection',
                'CFF/CID/private subroutines','real Fira','caller arguments','callee-saved registers','all touched pages'],
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in
            ('kernel/font_init.c','kernel/font_init.h','tests/kernel_font_init.py')})
    (BUILD/'font_init_summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))

if __name__=='__main__':main()
