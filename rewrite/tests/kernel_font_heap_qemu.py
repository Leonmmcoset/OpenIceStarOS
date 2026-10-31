"""Real guest malloc/free underneath the recovered font arena implementation."""
import hashlib
import json
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(BUILD/'test-deps'),str(ROOT/'tools'),str(ROOT/'tests')]
from kernel_image import Fat16,require
from kernel_fat_mutation_qemu import boot
from tests.kernel_font_heap import SLOTS
ENTRIES=[(n,s,e) for n,_,s,e in SLOTS]

def fixture(source):
    data=Fat16(source).replace_file((BUILD/'font_heap_guest.x').read_bytes(),b'ICESTAR    ',b'COLOR   X  ')
    return data,data,[]

def main():
    report=(0x48454131,1,1999,1,1998,1,1998,1,1,1,799,1,798,1,798,1,1,1,99)
    results=[]
    for label,path in [('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),('recovered',BUILD/'OpenIceStarOS_c_apps.vhd')]:
        results.append(boot(label,path,fixture_builder=fixture,suite='font_heap',entries=ENTRIES,
            expected_report=report,compact_changes=True,instruction_trace=True))
    require(results[0]['report']==results[1]['report'],'font arena guest reports differ')
    summary=dict(cases=results,installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
            (Path(__file__),ROOT/'tests/font_heap_guest.c')},complete_os_recovery=False)
    (BUILD/'font_heap_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('Three installed font arena entries use real malloc/free; chunk counts/reuse/cleanup match; full disks unchanged.')

if __name__=='__main__':main()
