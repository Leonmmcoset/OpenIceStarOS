"""Installed curve entries and complete outline-to-pixel chain in real QEMU."""
import hashlib
import json
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(BUILD/'test-deps'),str(ROOT/'tools'),str(ROOT/'tests')]
from kernel_image import Fat16,require
from kernel_fat_mutation_qemu import boot
from tests.kernel_font_flatten import SLOTS
ENTRIES=[(n,s,e) for n,_,s,e in SLOTS]+[
    ('original_raster_entry',0x83244c,0x832530),('original_edge_builder',0x831884,0x831ba8),
    ('original_raster_driver',0x830f24,0x831340)]

def fixture(source):
    data=Fat16(source).replace_file((BUILD/'font_flatten_guest.x').read_bytes(),b'ICESTAR    ',b'COLOR   X  ')
    return data,data,[]

def floats(coords):return struct.unpack('<'+'I'*len(coords),struct.pack('<'+'f'*len(coords),*coords))

def expected():
    values=[0x464c5431,0x7f800001,0x80000000,1,4]
    values+=floats([.5,3.5,2,6,4.5,7.5,8,8])
    values+=[2,*floats([4,6,8,0]),1,0,0,1,2,3,4]
    values+=floats([0,0,8,8,0,0,0,0,4,6,8,0,0,0])
    # Height <8 selects fifteen vertical samples, each weighing 17. The real
    # original REP MOVSD also copies 24 zero padding bytes after each row.
    values+=([0]+[255]*6+[0]+[0]*24)*4
    return tuple(values)

def main():
    results=[]
    for label,path in [('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),('recovered',BUILD/'OpenIceStarOS_c_apps.vhd')]:
        results.append(boot(label,path,fixture_builder=fixture,suite='font_flatten',entries=ENTRIES,
            expected_report=expected(),compact_changes=True,instruction_trace=True))
    require(results[0]['report']==results[1]['report'],'flatten/raster guest reports differ')
    summary=dict(cases=results,installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
            (Path(__file__),ROOT/'tests/font_flatten_guest.c')},complete_os_recovery=False)
    (BUILD/'font_flatten_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('Four installed curve/flatten entries and all 128 raster bytes match expectations; whole disks unchanged.')

if __name__=='__main__':main()
