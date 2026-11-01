"""Original and integrated PNG entries exercised by a real diagnostic X."""
import hashlib
import json
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(BUILD/'test-deps'),str(ROOT/'tools'),str(ROOT/'tests')]
from kernel_image import Fat16,require
from kernel_fat_mutation_qemu import boot
from tests.kernel_png import SLOTS,paeth
ENTRIES=[(n,s,e) for n,_,s,e in SLOTS]
def fixture(source):
    data=Fat16(source).replace_file((BUILD/'png_guest.x').read_bytes(),b'ICESTAR    ',b'COLOR   X  ')
    return data,data,[]
def fnv(data):
    v=2166136261
    for b in data:v=(v^b)*16777619&0xffffffff
    return v
def expected():
    line=[]
    for i in range(16):line.append((i*13+7+paeth(line[i-3] if i>=3 else 0,i*5+3,(i-3)*5+3 if i>=3 else 0))&255)
    return (0x504e4731,1,0,3,2,32,4,8,32,3,3,0,0,0,24,fnv(bytes(range(1,25))),0,0,0,1,0,50,fnv(line),0xa0,0x60,0xad,0xad,0)
def main():
    results=[]
    for label,path in [('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),('recovered',BUILD/'OpenIceStarOS_c_apps.vhd')]:
        results.append(boot(label,path,fixture_builder=fixture,suite='png',entries=ENTRIES,
            expected_report=expected(),compact_changes=True,instruction_trace=True))
    require(results[0]['report']==results[1]['report'],'PNG guest reports differ')
    summary=dict(cases=results,installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (Path(__file__),ROOT/'tests/png_guest.c')},complete_os_recovery=False)
    (BUILD/'png_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('All 23 PNG entries/body traces, pixels, filters and lifecycle match; whole disks unchanged.')
if __name__=='__main__':main()
