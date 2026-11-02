"""Trace real window create/close through the existing full guest lifecycle."""
import hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(ROOT/'tools'),str(ROOT/'tests'),str(BUILD/'test-deps')]
from kernel_image import Fat16,require
from kernel_fat_mutation_qemu import boot
from tests.kernel_window_lifecycle import SLOTS
from tests.kernel_window_services_qemu import expected
from tests.kernel_window_services import SLOTS as SERVICES
ENTRIES=[(n,s,e) for n,_,s,e in [*SLOTS,*SERVICES]]
def fixture(source):
    data=Fat16(source).replace_file((BUILD/'window_lifecycle_guest.x').read_bytes(),b'ICESTAR    ',b'COLOR   X  ')
    return data,data,[]
def main():
    results=[]
    for label,path in [('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),('recovered',BUILD/'OpenIceStarOS_c_apps.vhd')]:
        results.append(boot(label,path,fixture_builder=fixture,suite='window_lifecycle',entries=ENTRIES,
            expected_report=(0x574c4332,*([1]*15),*expected()),compact_changes=True,instruction_trace=True))
    require(results[0]['report']==results[1]['report'],'window lifecycle reports differ')
    summary=dict(cases=results,installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),
        new_entries=2,neighboring_entries=31,complete_os_recovery=False,
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
            (Path(__file__),ROOT/'tests/window_lifecycle_guest.c',ROOT/'tests/window_services_guest.c',ROOT/'tests/kernel_window_services_qemu.py')})
    (BUILD/'window_lifecycle_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('Window create/close and 31 neighboring bodies execute real heap, captions, surfaces and lifecycle; independent reports and whole disks match.')
if __name__=='__main__':main()
