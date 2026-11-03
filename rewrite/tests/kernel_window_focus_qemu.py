"""Real window focus/order/callback and title rectangles on disposable disks."""
import hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(ROOT/'tools'),str(ROOT/'tests'),str(BUILD/'test-deps')]
from kernel_image import Fat16,require
from kernel_fat_mutation_qemu import boot
from tests.kernel_window_focus import SLOTS
from tests.kernel_window_services_qemu import expected
def fixture(source):
    data=Fat16(source).replace_file((BUILD/'window_focus_guest.x').read_bytes(),b'ICESTAR    ',b'COLOR   X  ')
    return data,data,[]
def main():
    report=(0x57464331,*([1]*7),*((1,2,3,0,4)*5),1,1,*expected())
    cases=[boot(label,path,fixture_builder=fixture,suite='window_focus',
        entries=[(n,s,e) for n,_,s,e in SLOTS],expected_report=report,compact_changes=True,instruction_trace=True)
        for label,path in [('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),('recovered',BUILD/'OpenIceStarOS_c_apps.vhd')]]
    require(cases[0]['report']==cases[1]['report'],'window-focus reports differ')
    summary=dict(cases=cases,installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),new_entries=4,
        complete_os_recovery=False,sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
        (Path(__file__),ROOT/'tests/window_focus_guest.c',ROOT/'tests/window_services_guest.c',ROOT/'tests/kernel_window_services_qemu.py')})
    (BUILD/'window_focus_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('Four focus/title-hit bodies execute real surface ordering, caption, callbacks and notifications; reports and whole disks match.')
if __name__=='__main__':main()
