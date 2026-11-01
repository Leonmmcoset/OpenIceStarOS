"""Real raster-driver coverage and installed scan/sort entries in QEMU."""
import hashlib
import json
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(BUILD/'test-deps'),str(ROOT/'tools'),str(ROOT/'tests')]
from kernel_image import Fat16,require
from kernel_fat_mutation_qemu import boot
from tests.kernel_font_scan import SLOTS,coverage
from tests.kernel_font_context import fbits
ENTRIES=[(n,s,e) for n,_,s,e in SLOTS]+[('original_raster_driver',0x830f24,0x831340)]

def fixture(source):
    data=Fat16(source).replace_file((BUILD/'font_scan_guest.x').read_bytes(),b'ICESTAR    ',b'COLOR   X  ')
    return data,data,[]

def expected():
    values=[0x53434e31,1,0,1664,1408,fbits(3.5),1,1999]
    values+=list(coverage(8,[(256,1),(1792,-1)],63,bytes([0xa5])*8))
    values+=list(range(25))
    # Ascending thirteen-item input: the first median partition moves key 6
    # to slot zero; both partitions are <=12 and intentionally left unsorted.
    values += [6,1,2,3,4,5,0,7,8,9,10,11,12]
    values+=list(range(25))
    # Four vertical samples each weigh floor(255/4)=63. Boundary pixels have
    # 3/4 coverage per sample; the original copies width DWORDs per row.
    row=[0,188,252,252,252,252,188,0]+[0]*24
    values+=row*4
    return tuple(value&0xffffffff for value in values)

def main():
    results=[]
    for label,path in [('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),('recovered',BUILD/'OpenIceStarOS_c_apps.vhd')]:
        results.append(boot(label,path,fixture_builder=fixture,suite='font_scan',entries=ENTRIES,
            expected_report=expected(),compact_changes=True,instruction_trace=True))
    require(results[0]['report']==results[1]['report'],'scan/raster guest reports differ')
    summary=dict(cases=results,installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
            (Path(__file__),ROOT/'tests/font_scan_guest.c')},complete_os_recovery=False)
    (BUILD/'font_scan_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('Five installed scan/sort entries and actual raster-driver pixels match independent expectations; full disks unchanged.')

if __name__=='__main__':main()
