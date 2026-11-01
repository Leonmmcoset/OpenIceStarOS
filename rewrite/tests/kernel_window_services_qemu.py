"""31 real UI services with task launches, callbacks and real window lifecycle."""
import hashlib,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(ROOT/'tools'),str(ROOT/'tests'),str(BUILD/'test-deps')]
from kernel_image import Fat16,require
from kernel_fat_mutation_qemu import boot
from tests.kernel_window_services import SLOTS
ENTRIES=[(n,s,e) for n,_,s,e in SLOTS]
def fixture(source):
    data=Fat16(source).replace_file((BUILD/'window_services_guest.x').read_bytes(),b'ICESTAR    ',b'COLOR   X  ')
    return data,data,[]
def signature(data):
    h=2166136261
    for b in data:h=(h^b)*16777619&0xffffffff
    return h
def expected():
    # Independent classic VGA software RGB entries selected by 32bpp boot.
    palette={0:0x000000,7:0xffffff,8:0xc6c6c6,15:0x848484}
    frame=bytearray(b'\x7e'*(256*192*4));border=bytearray(frame);fill=bytearray(frame)
    def rect(data,x0,y0,x1,y1,color):
        for y in range(min(y0,y1),max(y0,y1)+1):
            for x in range(min(x0,x1),max(x0,x1)+1):struct.pack_into('<I',data,(y*256+x)*4,color)
    for a,b,c,d,color in [(6,6,26,6,palette[15]),(5,6,5,22,palette[15]),(5,23,26,23,palette[7]),(27,6,27,23,palette[7]),(7,7,25,7,palette[0]),(6,7,6,21,palette[0]),(6,22,25,22,palette[8]),(26,7,26,22,palette[8]),(7,8,25,21,0x35)]:rect(frame,a,b,c,d,color)
    for x0,y0,x1,y1,color in [(8,9,25,21,palette[7]),(9,10,24,20,palette[15])]:
        for a,b,c,d in [(x0,y0,x1,y0),(x0,y1,x1,y1),(x0,y0,x0,y1),(x1,y0,x1,y1)]:rect(border,a,b,c,d,color)
    for i in range(108):struct.pack_into('<I',fill,i*4,0x12345678)
    return (0x57494e31,1,0x6c69616d,0,1,1,1,0,1,0,1,0,signature(frame),signature(border),0,1,0,4095,11,22,33,44,55,66,77,88,99,signature(fill),0,
        1,0,0,0,1,1,103,84,103,84,1,0,0,1,1,0,1,0,1,0,0,0)
def main():
    results=[]
    for label,path in [('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),('recovered',BUILD/'OpenIceStarOS_c_apps.vhd')]:
        results.append(boot(label,path,fixture_builder=fixture,suite='window_services',entries=ENTRIES,expected_report=expected(),compact_changes=True,instruction_trace=True))
    require(results[0]['report']==results[1]['report'],'window reports differ')
    summary=dict(cases=results,installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),complete_os_recovery=False,
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (Path(__file__),ROOT/'tests/window_services_guest.c')})
    (BUILD/'window_services_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n');print('All 31 installed bodies execute real UI services; independent report and whole disks match.')
if __name__=='__main__':main()
