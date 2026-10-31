"""Execute all recovered CFF context helpers in original/C QEMU kernels."""
import hashlib
import json
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(BUILD/'test-deps'),str(ROOT/'tools'),str(ROOT/'tests')]
from kernel_image import Fat16,require
from kernel_fat_mutation_qemu import boot
from tests.kernel_font_context import SLOTS,fbits,vertex_bytes
ENTRIES=[(n,s,e) for n,_,s,e in SLOTS]+[('original_type2_interpreter',0x82e6f0,0x82f756)]

def fixture(source):
    data=Fat16(source).replace_file((BUILD/'font_context_guest.x').read_bytes(),
        b'ICESTAR    ',b'COLOR   X  ')
    return data,data,[]

def expected():
    values=[0x43545831]
    vertices=b''.join(vertex_bytes(*v)+b'\xa5' for v in
        [(1,1,2,0,0,0,0),(2,4,6,0,0,0,0),(4,10,12,5,7,7,9),(2,1,2,0,0,0,0)])
    for mode in (0,1):
        values += [mode,mode,fbits(1),fbits(2),fbits(10),fbits(12)]
        values += [1,10,-3,12] if mode else [0]*4
        values += [1,4]
        values += list(struct.unpack('<14I',bytes([0xa5])*56 if mode else vertices))
    # moveto(0,0), cubic controls (10,20),(40,60), end(90,120), close.
    values += [3,0,0,90,120]
    return tuple(v&0xffffffff for v in values)

def main():
    results=[]
    for label,path in [('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),
                       ('recovered',BUILD/'OpenIceStarOS_c_apps.vhd')]:
        results.append(boot(label,path,fixture_builder=fixture,suite='font_context',
            entries=ENTRIES,expected_report=expected(),compact_changes=True,instruction_trace=True))
    require(results[0]['report']==results[1]['report'],'CFF context reports differ')
    summary=dict(cases=results,installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
            (Path(__file__),ROOT/'tests/font_context_guest.c')},complete_os_recovery=False)
    (BUILD/'font_context_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('All six CFF context helpers match independent geometry; Type-2 bytecode executes; full disks unchanged.')

if __name__=='__main__':main()
