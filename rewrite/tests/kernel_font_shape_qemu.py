"""Observe installed TrueType/CFF shapes via real X/PS2/heap guest calls."""
import hashlib
import json
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(BUILD/'test-deps'),str(ROOT/'tools'),str(ROOT/'tests')]
from kernel_image import Fat16,require
from kernel_fat_mutation_qemu import boot
from tests.kernel_font_shape import SLOTS,geometry
from tests.kernel_font_glyph import real_info
from tests.kernel_font_cmap import independent_map
ENTRIES=[(n,s,e) for n,_,s,e in SLOTS]

def fixture(source):
    data=Fat16(source).replace_file((BUILD/'font_shape_guest.x').read_bytes(),b'ICESTAR    ',b'COLOR   X  ')
    return data,data,[]

def simple_geometry(font,tables,fmt,glyph):
    # Independent table parser: decode point arrays, then construct a closed
    # on/off-curve ring. It does not execute the old or recovered shape code.
    location=struct.unpack_from('>H' if fmt==0 else '>I',font,tables[b'loca']+glyph*(2 if fmt==0 else 4))[0]*(2 if fmt==0 else 1)
    at=tables[b'glyf']+location;contours=struct.unpack_from('>h',font,at)[0]
    assert contours>0
    ends=struct.unpack_from('>'+'H'*contours,font,at+10)
    at+=10+2*contours;instructions=struct.unpack_from('>H',font,at)[0];at+=2+instructions
    flags=[]
    while len(flags)<=ends[-1]:
        flag=font[at];at+=1;repeat=1
        if flag&8:repeat+=font[at];at+=1
        flags.extend([flag]*repeat)
    axes=[]
    for axis in (0,1):
        values=[];value=0
        for flag in flags:
            if flag&(2<<axis):
                delta=font[at]*(1 if flag&(16<<axis) else -1);at+=1
            elif flag&(16<<axis):delta=0
            else:delta=struct.unpack_from('>h',font,at)[0];at+=2
            value=(value+delta+32768)%65536-32768;values.append(value)
        axes.append(values)
    points=list(zip(*axes,[bool(f&1) for f in flags]));rings=[];first=0
    for end in ends:rings.append(points[first:end+1]);first=end+1
    return geometry(rings)

def signature(vertices):
    value=2166136261
    for kind,*coords in vertices:
        for b in struct.pack('<B'+'h'*len(coords),kind,*coords):value=((value^b)*16777619)&0xffffffff
    return len(vertices),value

def expected(source):
    font=Fat16(source).read_file(b'ICESTAR    ',b'FONT       ',b'FIRA    TTF')
    info,tables,count,fmt,selected=real_info(font);mapping=independent_map(font,selected)
    outlines=[simple_geometry(font,tables,fmt,g) for g in (1,14,mapping[65])]
    square=geometry([[(0,0,True),(100,0,True),(100,100,True),(0,100,True)]])
    moved=[(kind,x+31,y-27) for kind,x,y in square]
    scaled=[(kind,int(.5*(.5*x+20)),int(.5*(.5*y-10))) for kind,x,y in moved]
    cubic=[(1,0,0),(4,90,120,10,20,40,60),(2,0,0)]
    outlines += [square,moved,scaled,[],cubic,cubic,[]]
    return (0x53485031,1,1,*(value for outline in outlines for value in signature(outline)))

def main():
    results=[]
    for label,path in [('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),('recovered',BUILD/'OpenIceStarOS_c_apps.vhd')]:
        results.append(boot(label,path,fixture_builder=fixture,suite='font_shape',entries=ENTRIES,
            expected_report=expected(path.read_bytes()),compact_changes=True,instruction_trace=True))
    require(results[0]['report']==results[1]['report'],'shape guest reports differ')
    summary=dict(cases=results,installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
            (Path(__file__),ROOT/'tests/font_shape_guest.c')},complete_os_recovery=False)
    (BUILD/'font_shape_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('Four installed shape entries match independent geometry; real heap/X loader, recursive transforms, CFF output and whole disks checked.')

if __name__=='__main__':main()
