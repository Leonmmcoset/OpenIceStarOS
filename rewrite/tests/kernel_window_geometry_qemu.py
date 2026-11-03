"""Actual window maximize/layout/restore with real heap, drawing and callbacks."""
import hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(ROOT/'tools'),str(ROOT/'tests'),str(BUILD/'test-deps')]
from kernel_image import Fat16,require
from kernel_fat_mutation_qemu import boot
from tests.kernel_window_geometry import SLOTS
from tests.kernel_window_services_qemu import expected
def fixture(source):
    data=Fat16(source).replace_file((BUILD/'window_geometry_guest.x').read_bytes(),b'ICESTAR    ',b'COLOR   X  ')
    return data,data,[]
def main():
    report=[0x57474d31,1]
    for geometry in ((0,0,1024,768),(0,0,1024,740),(32,0,992,768),(0,0,992,768),(0,28,1024,740),(0,0,1024,768)):
        report.extend((1,*geometry,1,1,1,1))
    report.extend((1,*expected()))
    cases=[boot(label,path,fixture_builder=fixture,suite='window_geometry',entries=[(n,s,e) for n,_,s,e in SLOTS],
        expected_report=tuple(report),compact_changes=True,instruction_trace=True) for label,path in
        [('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),('recovered',BUILD/'OpenIceStarOS_c_apps.vhd')]]
    require(cases[0]['report']==cases[1]['report'],'window-geometry reports differ')
    summary=dict(cases=cases,installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),new_entries=3,
        complete_os_recovery=False,sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
        (Path(__file__),ROOT/'tests/window_geometry_guest.c',ROOT/'tests/window_services_guest.c',ROOT/'tests/kernel_window_services_qemu.py')})
    (BUILD/'window_geometry_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('Three geometry bodies execute real heap growth, frame drawing, six taskbar layouts and callbacks; reports and whole disks match.')
if __name__=='__main__':main()
