"""Run seven recovered outline/bounds entries through real guest APIs."""
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(BUILD/'test-deps'),str(ROOT/'tools'),str(ROOT/'tests')]
from kernel_image import Fat16,require
from kernel_fat_mutation_qemu import boot
from tests.kernel_font_outline import SLOTS,vertex_bytes
from tests.kernel_font_glyph import real_info
from tests.kernel_font_cmap import independent_map
ENTRIES=[(n,s,e) for n,_,s,e in SLOTS]+[('original_type2_interpreter',0x82e6f0,0x82f756)]

def fixture(source):
    data=Fat16(source).replace_file((BUILD/'font_outline_guest.x').read_bytes(),b'ICESTAR    ',b'COLOR   X  ')
    return data,data,[]

def expected(source):
    values=[0x4f555431]
    for off in (0,1):
        for start in (0,1):
            raw=bytearray([0xa5])*28
            vertices=[(3,40,50,50,60)] if off and start else []
            vertices.append((3,10,20,30,40) if start else (3,10,20,50,60) if off else (2,10,20,0,0))
            for i,v in enumerate(vertices):
                packed=vertex_bytes(*v);raw[i*14:i*14+8]=packed[:8];raw[i*14+12]=packed[12]
            values += [len(vertices),*struct.unpack('<7I',raw)]
    values += [0,0,0,5,0,2,136,0,9,136,0,9]
    values += [3,0,0,10,20]*3
    values += [1,1]
    font=Fat16(source).read_file(b'ICESTAR    ',b'FONT       ',b'FIRA    TTF')
    info,tables,count,fmt,selected=real_info(font);mapping=independent_map(font,selected)
    for entry,glyph in enumerate((34,34,mapping[65],mapping[65])):
        offset=struct.unpack_from('>H' if fmt==0 else '>I',font,tables[b'loca']+glyph*(2 if fmt==0 else 4))[0]*(2 if fmt==0 else 1)
        x0,y0,x1,y1=struct.unpack_from('>4h',font,tables[b'glyf']+offset+2)
        dx,dy=(.75,-.5) if not entry&1 else (0,0)
        values += [math.floor(x0/32+dx),math.floor(-y1/16+dy),math.ceil(x1/32+dx),math.ceil(-y0/16+dy)]
    return tuple(v&0xffffffff for v in values)

def main():
    results=[]
    for label,path in [('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),('recovered',BUILD/'OpenIceStarOS_c_apps.vhd')]:
        results.append(boot(label,path,fixture_builder=fixture,suite='font_outline',entries=ENTRIES,
            expected_report=expected(path.read_bytes()),compact_changes=True,instruction_trace=True))
    require(results[0]['report']==results[1]['report'],'outline guest reports differ')
    summary=dict(cases=results,installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
            (Path(__file__),ROOT/'tests/font_outline_guest.c')},complete_os_recovery=False)
    (BUILD/'font_outline_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('Seven installed outline/bounds entries match geometry; local/global/CID subroutines execute; full disks unchanged.')

if __name__=='__main__':main()
