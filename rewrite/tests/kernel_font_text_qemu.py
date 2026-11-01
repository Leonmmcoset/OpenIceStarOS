"""20 real original/recovered name and TrueType text bodies in QEMU."""
import hashlib,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(ROOT/'tools'),str(ROOT/'tests'),str(BUILD/'test-deps')]
from kernel_image import Fat16,require
from kernel_fat_mutation_qemu import boot
from tests.kernel_font_names import SLOTS as NAMES
from tests.kernel_font_text import SLOTS as TEXT
ENTRIES=[(n,s,e) for n,_,s,e in NAMES+TEXT]
def fixture(source):
    data=Fat16(source).replace_file((BUILD/'font_text_guest.x').read_bytes(),b'ICESTAR    ',b'COLOR   X  ')
    return data,data,[]
def pixel_hash(bg,clip=False):
    data=bytearray(b'\x5a'*8192);fg=0xe15397
    color=sum(((((fg>>s)&255)*255+((bg>>s)&255))//256)<<s for s in (0,8,16))
    for y in range(4,6 if clip else 8):
        for x in range(5,8 if clip else 11):struct.pack_into('<I',data,(y*64+x)*4,color)
    h=2166136261
    for b in data:h=(h^b)*16777619&0xffffffff
    return h
def expected():
    opaque,clip,alpha=pixel_hash(0x173759),pixel_hash(0x173759,True),pixel_hash(0x5a5a5a)
    return (0x54455831,1,7,1,1,2858,14,1,1,0,0,0,1,0x1f600,4,6,4,1,0xfffffffc,opaque,clip,alpha,opaque,opaque,0,opaque,clip,alpha)
def main():
    results=[]
    for label,path in [('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),('recovered',BUILD/'OpenIceStarOS_c_apps.vhd')]:
        results.append(boot(label,path,fixture_builder=fixture,suite='font_text',entries=ENTRIES,expected_report=expected(),compact_changes=True,instruction_trace=True))
    require(results[0]['report']==results[1]['report'],'font reports differ')
    summary=dict(cases=results,installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),complete_os_recovery=False,
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (Path(__file__),ROOT/'tests/font_text_guest.c')})
    (BUILD/'font_text_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n');print('All 20 installed bodies and independent Unicode/name/pixel reports match; whole disks unchanged.')
if __name__=='__main__':main()
