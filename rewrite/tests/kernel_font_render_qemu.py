"""Eleven installed bitmap/edge entries through actual guest X/PS2 calls."""
import hashlib
import json
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(BUILD/'test-deps'),str(ROOT/'tools'),str(ROOT/'tests')]
from kernel_image import Fat16,require
from kernel_fat_mutation_qemu import boot
from tests.kernel_font_render import SLOTS
ENTRIES=[(n,s,e) for n,_,s,e in SLOTS]+[('original_scan_driver',0x830f24,0x831340)]

def fixture(source):
    data=Fat16(source).replace_file((BUILD/'font_render_guest.x').read_bytes(),b'ICESTAR    ',b'COLOR   X  ')
    return data,data,[]

def signature(data):
    value=2166136261
    for b in data:value=((value^b)*16777619)&0xffffffff
    return value

def expected():
    # The original copies 4*width bytes with stride width when allocating a
    # glyph bitmap. Its 24 visible bytes are followed by 18 zero padding bytes.
    allocated=signature(bytes([255])*24+bytes(18))
    made=signature((bytes([255])*6+bytes(26))*4)
    return tuple([0x524e4431]+[1,6,4,1,-4&0xffffffff,allocated]*4+[made]*4)

def main():
    results=[]
    for label,path in [('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),('recovered',BUILD/'OpenIceStarOS_c_apps.vhd')]:
        results.append(boot(label,path,fixture_builder=fixture,suite='font_render',entries=ENTRIES,
            expected_report=expected(),compact_changes=True,instruction_trace=True))
    require(results[0]['report']==results[1]['report'],'bitmap/edge guest reports differ')
    summary=dict(cases=results,installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
            (Path(__file__),ROOT/'tests/font_render_guest.c')},complete_os_recovery=False)
    (BUILD/'font_render_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('Eleven installed render/bitmap entries match independent geometry/pixels; whole disks unchanged.')

if __name__=='__main__':main()
