"""Original/ordinary/installed glyph bounds and kerning, without helper stubs."""
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
from tests.kernel_font_cmap import Machine as CmapMachine,independent_map,words
BASE,DATA,BUFFER,OUTPUT,FONT,MASK=0x800000,0x600000,0x500000,0x510000,0xa00000,0xffffffff
SLOTS = [['font_vertex', 'kernel_font_vertex_82cf10', 8572688, 8572752], ['font_glyph_offset', 'kernel_font_glyph_offset_82cf50', 8572752, 8573026], ['font_glyph_box', 'kernel_font_glyph_box_82d062', 8573026, 8573276], ['font_codepoint_box', 'kernel_font_codepoint_box_82d15c', 8573276, 8573348], ['font_glyph_empty', 'kernel_font_glyph_empty_82d1a4', 8573348, 8573514], ['font_cff_box', 'kernel_font_cff_box_82f80a', 8583178, 8583434], ['font_kern_length', 'kernel_font_kern_length_82fa50', 8583760, 8583882], ['font_kern_table', 'kernel_font_kern_table_82faca', 8583882, 8584218], ['font_legacy_kern', 'kernel_font_legacy_kern_82fc1a', 8584218, 8584488], ['font_coverage', 'kernel_font_coverage_82fd28', 8584488, 8584894], ['font_class', 'kernel_font_class_82febe', 8584894, 8585252], ['font_gpos_kern', 'kernel_font_gpos_kern_830024', 8585252, 8586306], ['font_glyph_kern', 'kernel_font_glyph_kern_830442', 8586306, 8586408], ['font_codepoint_kern', 'kernel_font_codepoint_kern_8304a8', 8586408, 8586512], ['font_codepoint_metrics', 'kernel_font_codepoint_metrics_830510', 8586512, 8586570]]
VOID={0,14}


def compile_variants(original,installed):
    objects=[]
    for module in ('glyph','kern'):
        obj=BUILD/f'kernel_font_{module}_preview.o'
        run(['gcc',*FLAGS,'-c',f'kernel/font_{module}.c','-o',str(obj)])
        objects.append(str(obj))
    outputs,sizes=[],{}
    external=('kernel_font_u16_82be14','kernel_font_s16_82be34','kernel_font_u32_82be52',
        'kernel_font_charstring_82e6f0','kernel_font_glyph_index_82ca7c','kernel_font_hmetrics_82f958')
    provides=''.join(f'PROVIDE({s} = 0x{s.rsplit("_",1)[1]});\n' for s in external)
    for fixed in (False,True):
        label='fixed' if fixed else 'ordinary'
        script,elf=BUILD/f'font_glyph_{label}.ld',BUILD/f'font_glyph_{label}.elf'
        sections='\n'.join(f'.{n} {s:#x} : {{ *(.text.{sym}) }}\n'
            f'ASSERT({sym} == {s:#x} && SIZEOF(.{n}) <= {e-s}, "slot overflow")'
            for n,sym,s,e in SLOTS) if fixed else '. = 0x300000; .text : { *(.text*) }'
        script.write_text('OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_font_vertex_82cf10)\n'+provides+
            'SECTIONS {\n'+sections+'\n/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n'
            '.unexpected : { *(*) }\nASSERT(SIZEOF(.unexpected)==0, "unexpected section")\n}\n')
        run(['ld','-m','elf_i386','-T',str(script),'-o',str(elf),*objects])
        symbols={line.split()[-1]:int(line.split()[0],16) for line in
            run(['nm',str(elf)],capture_output=True,text=True).stdout.splitlines()}
        if fixed:
            candidate=bytearray(original)
            for n,sym,s,e in SLOTS:
                path=BUILD/f'font_glyph_preview.{n}.bin'
                run(['objcopy','-O','binary','-j','.'+n,str(elf),str(path)])
                body=path.read_bytes();assert 0<len(body)<=e-s
                candidate[s-BASE:e-BASE]=body+b'\x90'*(e-s-len(body))
                sizes[n]=dict(compiled_bytes=len(body),slot_bytes=e-s)
            if installed:
                production=(BUILD/'openicestaros_kernel.x').read_bytes()
                for _,_,s,e in SLOTS:assert production[s-BASE:e-BASE]==candidate[s-BASE:e-BASE]
                candidate=production
            outputs.append((label,bytes(candidate),BASE,[symbols[sym] for _,sym,_,_ in SLOTS]))
        else:
            path=BUILD/'font_glyph_ordinary.bin';run(['objcopy','-O','binary',str(elf),str(path)])
            outputs.append((label,path.read_bytes(),0x300000,[symbols[sym] for _,sym,_,_ in SLOTS]))
    return outputs,sizes


class Machine(CmapMachine):
    def execute(self,case):
        result=super().execute(case)
        return (None,*result[1:]) if case['function'] in VOID else result


def case(fn,args,setup=(),expected=None,memory=()):
    c=dict(function=fn,args=args,setup=list(setup),expected_memory=list(memory))
    if expected is not None:c['expected_return']=expected&MASK
    return c


def font_info(data=DATA,**fields):
    result=bytearray(160);struct.pack_into('<I',result,4,data)
    offsets=dict(glyphs=12,loca=16,head=20,glyf=24,hhea=28,hmtx=32,kern=36,gpos=40,cmap=48,loca_format=52,cff_size=64)
    for name,value in fields.items():struct.pack_into('<I',result,offsets[name],value&MASK)
    return bytes(result)


def font_tables(font):
    return {font[12+i*16:16+i*16]:struct.unpack_from('>I',font,20+i*16)[0]
        for i in range(struct.unpack_from('>H',font,4)[0])}


def real_info(font):
    tables=font_tables(font);cmap=tables[b'cmap'];selected=None
    for i in range(struct.unpack_from('>H',font,cmap+2)[0]):
        platform,encoding,relative=struct.unpack_from('>HHI',font,cmap+4+i*8)
        if platform==0 or platform==3 and encoding in (1,10):selected=cmap+relative
    count=struct.unpack_from('>H',font,tables[b'maxp']+4)[0]
    fmt=struct.unpack_from('>h',font,tables[b'head']+50)[0]
    info=font_info(data=FONT,glyphs=count,loca=tables[b'loca'],glyf=tables[b'glyf'],head=tables[b'head'],
        hhea=tables[b'hhea'],hmtx=tables[b'hmtx'],kern=tables.get(b'kern',0),gpos=tables.get(b'GPOS',0),
        cmap=selected,loca_format=fmt)
    return info,tables,count,fmt,selected


def glyph_scenarios(font):
    cases=[];rng=random.Random(0x82cf10)
    for i in range(100):
        args=[OUTPUT,*[rng.getrandbits(32) for _ in range(5)]]
        image=bytearray([0xa5]*16);image[12]=args[1]&255
        struct.pack_into('<4H',image,0,*(v&65535 for v in args[2:]))
        cases.append(case(0,args,memory=[(OUTPUT,bytes(image))]))
    for fmt in (0,1,-1,2,0x80000000):
        for first,last in ((0,0),(10,20),(10,10),(20,10),(65535,0),(0x80000001,0x80000002)):
            raw=bytearray(0x1000)
            struct.pack_into('>II',raw,128,first&MASK,last&MASK) if fmt else struct.pack_into('>HH',raw,128,first&65535,last&65535)
            for glyph in (-1,0,1,4,5,0x7fffffff,0x80000000):
                info=font_info(glyphs=1,loca=128,glyf=256,loca_format=fmt)
                setup=[(BUFFER,info),(DATA,bytes(raw))]
                # Differential expectations also cover negative glyphs, negative
                # formats and wrapped offsets instead of sanitizing the inputs.
                cases.append(case(1,[BUFFER,glyph],setup))
    for contours in (0,1,-1,32767,-32768):
        raw=bytearray(0x1000);struct.pack_into('>3H',raw,128,128,138,138)
        struct.pack_into('>5h',raw,512,contours,-123,-32768,32767,456)
        for mask in range(16):
            info=font_info(glyphs=2,loca=128,glyf=256,loca_format=0,cmap=1024)
            struct.pack_into('>HHH',raw,1024,0,262,0);raw[1030+65]=0;raw[1030+66]=1
            setup=[(BUFFER,info),(DATA,bytes(raw))]
            out=[OUTPUT+i*4 if mask&(1<<i) else 0 for i in range(4)]
            expect=[(p,words(v)) for p,v in zip(out,(-123,-32768,32767,456)) if p]
            for fn,arg in ((2,0),(3,65)):
                cases.append(case(fn,[BUFFER,arg,*out],setup,1,expect))
            cases.append(case(2,[BUFFER,1,*out],setup,0))
        cases.append(case(4,[BUFFER,0],setup,int(contours==0)))
        cases.append(case(4,[BUFFER,1],setup,1))
        # Each output may mutate the live font pointer or subsequent source.
        for output in (BUFFER+4,DATA+516,DATA+518,OUTPUT):
            cases.append(case(2,[BUFFER,0,0,output,OUTPUT+8,OUTPUT+12],setup))
    # Actual CFF Type-2 bytecode runs through the original interpreter, using
    # both original and recovered buffer/CFF helpers. No callbacks are stubbed.
    programs=[b'\x0e',bytes([139,139,21,239,139,5,139,239,5,39,139,5,14]),b'\x00',b'']
    for program in programs:
        index=struct.pack('>HB',1,1)+bytes([1,len(program)+1])+program
        info=bytearray(font_info(cff_size=1));struct.pack_into('<3I',info,68,DATA,0,len(index))
        setup=[(BUFFER,bytes(info)),(DATA,index)]
        for mask in range(16):
            outputs=[OUTPUT+i*4 if mask&(1<<i) else 0 for i in range(4)]
            box=(0,0,100,100) if len(program)>1 else (0,0,0,0)
            memory=[(p,words(v)) for p,v in zip(outputs,box) if p]
            cases.append(case(5,[BUFFER,0,*outputs],setup,5 if len(program)>1 else 0,memory))
            cases.append(case(2,[BUFFER,0,*outputs],setup,1,memory))
        cases.append(case(4,[BUFFER,0],setup,int(len(program)<=1)))
    info,tables,count,fmt,selected=real_info(font)
    offsets=[struct.unpack_from('>H' if fmt==0 else '>I',font,tables[b'loca']+i*(2 if fmt==0 else 4))[0]*(2 if fmt==0 else 1)
             for i in range(count+1)]
    for glyph in range(count):
        setup=[(BUFFER,info)];offset=tables[b'glyf']+offsets[glyph]
        empty=offsets[glyph]==offsets[glyph+1]
        cases.append(case(1,[BUFFER,glyph],setup,-1 if empty else offset))
        box=struct.unpack_from('>4h',font,offset+2) if not empty else ()
        memory=[(OUTPUT,words(*box))] if box else []
        cases.append(case(2,[BUFFER,glyph,OUTPUT,OUTPUT+4,OUTPUT+8,OUTPUT+12],setup,0 if empty else 1,memory))
        contours=struct.unpack_from('>h',font,offset)[0] if not empty else 0
        cases.append(case(4,[BUFFER,glyph],setup,int(empty or contours==0)))
    mapping=independent_map(font,selected)
    for cp in list(range(256))+[1024,65535,65536,0x10ffff]:
        glyph=mapping.get(cp,0);offset=tables[b'glyf']+offsets[glyph]
        empty=offsets[glyph]==offsets[glyph+1]
        box=struct.unpack_from('>4h',font,offset+2) if not empty else ()
        cases.append(case(3,[BUFFER,cp,OUTPUT,OUTPUT+4,OUTPUT+8,OUTPUT+12],[(BUFFER,info)],0 if empty else 1,
            [(OUTPUT,words(*box))] if box else []))
    return cases


def legacy(records,subtables=1,coverage=1):
    header=struct.pack('>9H',0,subtables,0,14+6*len(records),coverage,len(records),0,0,0)
    return header+b''.join(struct.pack('>HHh',*r) for r in records)


def coverage1(glyphs):return struct.pack('>HH',1,len(glyphs))+struct.pack('>'+'H'*len(glyphs),*glyphs)
def coverage2(ranges):return struct.pack('>HH',2,len(ranges))+b''.join(struct.pack('>HHH',*r) for r in ranges)
def class1(first,values):return struct.pack('>HHH',1,first,len(values))+struct.pack('>'+'H'*len(values),*values)
def class2(ranges):return struct.pack('>HH',2,len(ranges))+b''.join(struct.pack('>HHH',*r) for r in ranges)


def pair1(glyphs,sets,value1=4,value2=0):
    raw=bytearray(struct.pack('>5H',1,0,value1,value2,len(sets))+bytes(len(sets)*2))
    for i,records in enumerate(sets):
        struct.pack_into('>H',raw,10+i*2,len(raw))
        raw+=struct.pack('>H',len(records))+b''.join(struct.pack('>Hh',*r) for r in records)
    struct.pack_into('>H',raw,2,len(raw));raw+=coverage1(glyphs)
    return bytes(raw)


def pair2(coverage,definition1,definition2,rows,value1=4,value2=0):
    raw=bytearray(struct.pack('>8H',2,0,value1,value2,0,0,len(rows),len(rows[0])))
    raw+=b''.join(struct.pack('>'+'h'*len(row),*row) for row in rows)
    for field,body in ((2,coverage),(8,definition1),(10,definition2)):
        struct.pack_into('>H',raw,field,len(raw));raw+=body
    return bytes(raw)


def gpos(lookups,major=1,minor=0):
    raw=bytearray(struct.pack('>5H',major,minor,0,0,10))
    raw+=struct.pack('>H',len(lookups))+bytes(len(lookups)*2)
    for i,(lookup_type,subtables) in enumerate(lookups):
        struct.pack_into('>H',raw,12+i*2,len(raw)-10)
        begin=len(raw);raw+=struct.pack('>3H',lookup_type,0,len(subtables))+bytes(len(subtables)*2)
        for j,sub in enumerate(subtables):
            struct.pack_into('>H',raw,begin+6+j*2,len(raw)-begin);raw+=sub
    return bytes(raw)


def kern_scenarios(font):
    cases=[];records=[(0,1,-32768),(1,2,-30),(1,4,50),(3,5,0),(32768,65535,32767),(65535,65535,-1)]
    for subtables in (0,1,2):
        for coverage in (0,1,3,0x100):
            raw=legacy(records,subtables,coverage)
            setup=[(BUFFER,font_info(kern=64)),(DATA+64,raw)]
            valid=subtables!=0 and coverage==1
            cases.append(case(6,[BUFFER],setup,len(records) if valid else 0))
            for capacity in (-2147483648,-1,0,1,3,6,7,2147483647):
                count=min(capacity,len(records)) if valid else 0
                mem=[(OUTPUT,words(*(v for r in records[:max(count,0)] for v in r)))] if count>0 else []
                cases.append(case(7,[BUFFER,OUTPUT,capacity],setup,count,mem))
            for left in (0,1,2,3,32768,65535,65536,0xffffffff):
                for right in (0,1,2,4,5,65535,65536,0xffffffff):
                    key=((left<<16)|right)&MASK
                    expect=next((v for a,b,v in records if (a<<16)|b==key),0) if valid else 0
                    cases.append(case(8,[BUFFER,left,right],setup,expect))
            if valid:
                for output in (DATA+64,DATA+64+20,BUFFER+4):
                    cases.append(case(7,[BUFFER,output,len(records)],setup))
    for fn,args in ((6,[BUFFER]),(7,[BUFFER,OUTPUT,20]),(8,[BUFFER,1,2]),(11,[BUFFER,1,2]),
                    (12,[BUFFER,1,2]),(13,[BUFFER,0x80000000,0x80000000])):
        cases.append(case(fn,args,[(BUFFER,font_info())],0))
    coverage_specs=[(coverage1([]),{}),(coverage1([0,2,65,300,65535]),{0:0,2:1,65:2,300:3,65535:4}),
        (coverage2([(0,3,10),(65,70,20),(65530,65535,40)]),
         {**{i:i+10 for i in range(4)},**{i:i-65+20 for i in range(65,71)},**{i:i-65530+40 for i in range(65530,65536)}}),
        (coverage2([]),{}),(struct.pack('>H',3),{})]
    class_specs=[(class1(65,[0,1,2,65535]),{65:0,66:1,67:2,68:65535},0),
        (class1(65535,[1,2,3]),{65535:1,65536:2,65537:3},0),
        (class2([(0,3,10),(65,70,20),(65530,65535,40)]),
         {**dict.fromkeys(range(4),10),**dict.fromkeys(range(65,71),20),**dict.fromkeys(range(65530,65536),40)},0),
        (class2([]),{},0),(struct.pack('>H',3),{},-1)]
    points=list(range(-2,305))+[65529,65530,65535,65536,65537,0x7fffffff,-2147483648]
    for raw,mapping in coverage_specs:
        for glyph in points:cases.append(case(9,[DATA,glyph],[(DATA,raw)],mapping.get(glyph,-1)))
    for raw,mapping,missing in class_specs:
        for glyph in points:cases.append(case(10,[DATA,glyph],[(DATA,raw)],mapping.get(glyph,missing)))
    # Pair-set and class-matrix fixtures include real signed extreme advances.
    sub=pair1([1,3],[[(2,-70),(4,32767)],[(0,-32768),(5,33)]])
    matrix=pair2(coverage2([(1,3,0)]),class1(1,[1,2,1]),class2([(2,2,1),(4,5,2)]),
        [[7,8,9],[10,-90,12],[13,14,-32768]])
    known1={(1,2):-70,(1,4):32767,(3,0):-32768,(3,5):33}
    def expect_matrix(left,right):
        if not 1<=left<=3:return 0
        a={1:1,2:2,3:1}[left];b=1 if right==2 else 2 if right in (4,5) else 0
        return [[7,8,9],[10,-90,12],[13,14,-32768]][a][b]
    variants=[(gpos([(2,[sub])]),lambda a,b:known1.get((a,b),0)),
        (gpos([(2,[matrix])]),expect_matrix),
        (gpos([(1,[sub]),(2,[sub])]),lambda a,b:known1.get((a,b),0)),
        (gpos([(2,[pair1([9],[[(2,1)]]),sub])]),lambda a,b:1 if (a,b)==(9,2) else known1.get((a,b),0)),
        (gpos([(2,[pair1([1],[[]]),sub])]),lambda a,b:known1.get((a,b),0)),
        (gpos([(2,[pair1([1],[[(2,1)]],value1=1),sub])]),lambda a,b:0 if a==1 else known1.get((a,b),0)),
        (gpos([(2,[sub])],major=2),lambda a,b:0),
        (gpos([(2,[sub])],minor=1),lambda a,b:0),(gpos([]),lambda a,b:0)]
    cmap=bytearray(struct.pack('>HHH',0,262,0)+bytes(range(256)))
    for raw,expected in variants:
        info=font_info(gpos=64,kern=2048,cmap=3072)
        setup=[(BUFFER,info),(DATA+64,raw),(DATA+2048,legacy(records)),(DATA+3072,bytes(cmap))]
        for left in (0,1,2,3,4,9,65535,-1,0x7fffffff,-2147483648):
            for right in (0,1,2,4,5,6,65535,-1,0x7fffffff,-2147483648):
                for fn in (11,12):cases.append(case(fn,[BUFFER,left,right],setup,expected(left,right)))
        for left in (0,1,2,3,9,255,256):
            for right in (0,2,4,5,255,256):
                a=left if left<256 else 0;b=right if right<256 else 0
                cases.append(case(13,[BUFFER,left,right],setup,expected(a,b)))
    # Unknown pair formats and invalid class formats/counts terminate with 0.
    for broken in ('format','value2','class1','class2','count1','count2'):
        raw=bytearray(matrix)
        if broken=='format':struct.pack_into('>H',raw,0,3)
        elif broken=='value2':struct.pack_into('>H',raw,6,1)
        elif broken in ('class1','class2'):
            where=struct.unpack_from('>H',raw,8 if broken=='class1' else 10)[0]
            struct.pack_into('>H',raw,where,3)
        else:struct.pack_into('>H',raw,12 if broken=='count1' else 14,0)
        setup=[(BUFFER,font_info(gpos=64)),(DATA+64,gpos([(2,[bytes(raw),sub])]))]
        cases.append(case(11,[BUFFER,1,2],setup,0))
    # Missing class entries select class 0, even for negative glyphs on right.
    # The independent matrix expectations above include those cases.
    info,tables,count,fmt,selected=real_info(font)
    mapping=independent_map(font,selected)
    hcount=struct.unpack_from('>H',font,tables[b'hhea']+34)[0]
    for cp in list(range(256))+[1024,65535,65536,0x10ffff]:
        glyph=mapping.get(cp,0)
        advance=struct.unpack_from('>h',font,tables[b'hmtx']+4*min(glyph,hcount-1))[0]
        bearing=struct.unpack_from('>h',font,tables[b'hmtx']+(4*glyph+2 if glyph<hcount else 4*hcount+2*(glyph-hcount)))[0]
        for mask in (0,1,2,3):
            outputs=[OUTPUT if mask&1 else 0,OUTPUT+4 if mask&2 else 0]
            cases.append(case(14,[BUFFER,cp,*outputs],[(BUFFER,info)],memory=[(p,words(v)) for p,v in zip(outputs,(advance,bearing)) if p]))
    for left in (0,1,2,13,34,65,113,138,264,count-1):
        for right in (0,1,2,13,34,65,113,138,264,count-1):
            for fn in (8,11,12):cases.append(case(fn,[BUFFER,left,right],[(BUFFER,info)]))
    for left,right in ((65,86),(84,111),(87,97),(65,65),(32,65),(65535,65)):
        cases.append(case(13,[BUFFER,left,right],[(BUFFER,info)]))
    return cases


def main():
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--installed',action='store_true');args=parser.parse_args()
    started=time.monotonic();disk=Fat16((ROOT.parent/'IceStarOS_x86_32M.vhd').read_bytes());original=disk.read_kernel()
    assert hashlib.sha256(original).hexdigest()==ORIGINAL_SHA256
    font=disk.read_file(b'ICESTAR    ',b'FONT       ',b'FIRA    TTF')
    variants,sizes=compile_variants(original,args.installed)
    specs=[('original',original,BASE,[s for _,_,s,_ in SLOTS]),*variants]
    machines=[Machine(*spec,original,font) for spec in specs]
    cases=glyph_scenarios(font)+kern_scenarios(font)
    coverage={n:0 for n,_,_,_ in SLOTS}
    for index,c in enumerate(cases):
        try:
            expected=machines[0].execute(c)
            for machine in machines[1:]:assert machine.execute(c)==expected,(machine.label,expected)
        except Exception:
            print('Failing case',index,{k:v for k,v in c.items() if k!='setup'},flush=True);raise
        coverage[SLOTS[c['function']][0]]+=1
    summary=dict(installed=args.installed,functions=len(SLOTS),scenarios=len(cases),executions=len(cases)*3,
        slots=sizes,scenarios_by_function=coverage,seconds=round(time.monotonic()-started,3),
        boundary_scope='All helpers execute; original CFF Type-2 interpreter remains a dependency',
        checks=['independent complete Fira loca/glyf header expectations','synthetic legacy/GPOS independent advances',
            'output aliases and signed boundaries','caller arguments','callee-saved registers','all touched data pages'],
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in
            ('kernel/font_glyph.c','kernel/font_glyph.h','kernel/font_kern.c','kernel/font_kern.h',
             'tests/kernel_font_glyph.py','tests/kernel_font_cmap.py')})
    (BUILD/'font_glyph_summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))

if __name__=='__main__':main()
