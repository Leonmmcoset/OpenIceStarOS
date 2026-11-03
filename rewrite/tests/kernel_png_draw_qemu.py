"""Real FAT/PNG/DEFLATE/heap and independent 8/32-bit pixel checks in QEMU."""
import hashlib,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(ROOT/'tools'),str(ROOT/'tests'),str(BUILD/'test-deps')]
from kernel_image import Fat16,require,word
from kernel_fat_mutation_qemu import boot
from tests.kernel_png_draw import SLOTS,encoded,pattern

def resources():
    small=encoded(3,2,pattern(3,2),6)
    large=bytes(v for y in range(64) for x in range(96) for v in
        ((0xd0,0x40,0x20,255) if y<32 and x<48 else
         (0xe0,0xa0,0x20,255) if y<32 else
         (0x20,0x60,0xc0,255) if x<48 else (0x30,0xa0,0x50,255)))
    return [(b'PNGTST  PNG',small),(b'PNGVIEW PNG',encoded(96,64,large,6))]

def fixture(source):
    disk=Fat16(source);result=bytearray(disk.replace_file((BUILD/'png_draw_guest.x').read_bytes(),b'ICESTAR    ',b'COLOR   X  '))
    bpb=disk.partition_lba*512;fat_start=bpb+word(source,bpb+14)*512;fat_bytes=word(source,bpb+22)*512
    root_start=fat_start+source[bpb+16]*fat_bytes
    require(not any(disk.root[4*32:7*32]),'PNG root fixture slots unavailable')
    free=[c for c in range(2,min(len(disk.fat)//2,(disk.end-disk.data_start)//disk.cluster_bytes+2))
        if all(word(f,c*2)==0 for f in disk.fat_copies)]
    clusters=[]
    for i,(name,content) in enumerate(resources()):
        require(len(content)<=disk.cluster_bytes,'PNG resource exceeds one cluster')
        c=free[i];clusters.append(c)
        for copy in range(source[bpb+16]):struct.pack_into('<H',result,fat_start+copy*fat_bytes+c*2,0xffff)
        record=bytearray(32);record[:11]=name;record[11]=0x20
        struct.pack_into('<HI',record,26,c,len(content));result[root_start+(4+i)*32:root_start+(5+i)*32]=record
        at=disk.data_start+(c-2)*disk.cluster_bytes;result[at:at+len(content)]=content
    return bytes(result),bytes(result),clusters

def main():
    expected=(0x504e4731,*([1]*24))
    cases=[boot(label,path,fixture_builder=fixture,suite='png_draw',entries=[(n,s,e) for n,_,s,e in SLOTS],
        expected_report=expected,compact_changes=True,instruction_trace=True) for label,path in
        [('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),('recovered',BUILD/'OpenIceStarOS_c_apps.vhd')]]
    require(cases[0]['report']==cases[1]['report'],'PNG draw reports differ')
    summary=dict(cases=cases,installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),new_entries=3,
        resources={name.decode():hashlib.sha256(data).hexdigest() for name,data in resources()},
        checks=['real FAT lookup/read and actual PNG/DEFLATE/heap/pixel routines',
            'independent full-buffer 8/32-bit transparency, clipping, dithering and sentinel checks',
            'absolute file success and absolute/relative missing files',
            'all three entry/body traces, visible real window and unchanged whole disposable disks'],
        complete_os_recovery=False,sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
        (Path(__file__),ROOT/'tests/png_draw_guest.c')})
    (BUILD/'png_draw_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('Three PNG adapters execute actual FAT/codec/heap/pixel chains; independent 25-DWORD reports and whole disks match.')
if __name__=='__main__':main()
