"""Real stored/fixed/dynamic DEFLATE through all ten guest entry bodies."""
import hashlib
import json
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(BUILD/'test-deps'),str(ROOT/'tools'),str(ROOT/'tests')]
from kernel_image import Fat16,require
from kernel_fat_mutation_qemu import boot
from tests.kernel_inflate import SLOTS,fixed_stream
from tests.kernel_png_qemu import fnv
ENTRIES=[(n,s,e) for n,_,s,e in SLOTS]
def fixture(source):
    data=Fat16(source).replace_file((BUILD/'inflate_guest.x').read_bytes(),b'ICESTAR    ',b'COLOR   X  ')
    return data,data,[]
def expected():
    values=[0x44464c31]
    for raw in (b'OpenIce',b'ABABA',b'ABCDEFGH'*2):values += [0,0,0,fnv(raw),0xa5]
    values += [4,4,0x208,fnv(b''),0xa5,1,4]
    data=b'\x78\1'+fixed_stream([65,66,257,256],[1])+bytes(4);value=0
    for i in range(40):value|=((data[(1+i)//8]>>((1+i)&7))&1)<<(i&31)
    values += [value,41,2,4,5,6,0,1,2,3,0,0,0,0]
    return tuple(values)
def main():
    results=[]
    for label,path in [('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),('recovered',BUILD/'OpenIceStarOS_c_apps.vhd')]:
        results.append(boot(label,path,fixture_builder=fixture,suite='inflate',entries=ENTRIES,
            expected_report=expected(),compact_changes=True,instruction_trace=True))
    require(results[0]['report']==results[1]['report'],'DEFLATE guest reports differ')
    summary=dict(cases=results,installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (Path(__file__),ROOT/'tests/inflate_guest.c')},complete_os_recovery=False)
    (BUILD/'inflate_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('All ten installed DEFLATE entries/body instructions, three block types and output limits match; disks unchanged.')
if __name__=='__main__':main()
