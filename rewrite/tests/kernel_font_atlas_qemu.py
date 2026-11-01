"""Eleven installed font atlas entries through real X/PS2 guest calls."""
import hashlib
import json
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(BUILD/'test-deps'),str(ROOT/'tools'),str(ROOT/'tests')]
from kernel_image import Fat16,require
from kernel_fat_mutation_qemu import boot
from tests.kernel_font_atlas import SLOTS
ENTRIES=[(n,s,e) for n,_,s,e in SLOTS]
def fixture(source):
    data=Fat16(source).replace_file((BUILD/'font_atlas_guest.x').read_bytes(),b'ICESTAR    ',b'COLOR   X  ')
    return data,data,[]
def floats(*coords):return struct.unpack('<'+'I'*len(coords),struct.pack('<'+'f'*len(coords),*coords))
def expected():
    values=[0x41544c31,1,77,16,12,16,1,0,0xa5,2,3,2,3,1]
    values+=floats(0,0,-.25,-1/3,-.375)
    values += [7,0x101,0x101]
    dims=[(7,5),(7,5),(0,0),(7,5),(0,0),(7,5),(0,0)]
    for w,h in dims:values += [w,h]
    # Independent row layout: two 7px cells per row within 15px width;
    # the 16x3 cell wraps to row10 then exceeds the 11px inner height.
    for x,y in [(0,0),(7,0),(14,0),(0,5),(7,5),(7,5),(14,5)]:values += [x,y,1]
    values += [0,0,0]*3
    uv=(2/64,3/32,8/64,9/32)
    for positions in [(11.5,-4.5,17.5,1.5),(12,-4,18,2),
                      (11.5,-4.25,19.5,-.25),(12,-4,20,0)]:
        x0,y0,x1,y1=positions;values+=floats(x0,y0,uv[0],uv[1],x1,y1,uv[2],uv[3],17.75,-2.75)
    values += [77,16,12,16,1,1,1]
    return tuple(values)
def main():
    results=[]
    for label,path in [('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),('recovered',BUILD/'OpenIceStarOS_c_apps.vhd')]:
        results.append(boot(label,path,fixture_builder=fixture,suite='font_atlas',entries=ENTRIES,
            expected_report=expected(),compact_changes=True,instruction_trace=True))
    require(results[0]['report']==results[1]['report'],'atlas guest reports differ')
    summary=dict(cases=results,installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
            (Path(__file__),ROOT/'tests/font_atlas_guest.c')},complete_os_recovery=False)
    (BUILD/'font_atlas_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('Eleven installed atlas entries match independent dimensions/layout/quads; whole disks unchanged.')
if __name__=='__main__':main()
