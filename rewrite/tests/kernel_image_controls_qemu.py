"""Actual image controls and FAT/PNG/BMP/JPEG/ICO chains on disposable disks."""
import hashlib,io,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(ROOT/'tools'),str(ROOT/'tests'),str(BUILD/'test-deps')]
from kernel_image import Fat16,require,word
from kernel_fat_mutation_qemu import boot
from tests.kernel_image_controls import SLOTS
from tests.kernel_png import png
from PIL import Image
ENTRIES=[(n,s,e) for n,_,s,e in SLOTS]

def resources():
    png_bytes=png(8,8,6,8,(b'\0'+b'\x11\x22\x33\xff'*8)*8)
    raw=b'\x66\x55\x44'*64
    bmp=b'BM'+struct.pack('<IHHI',54+len(raw),0,0,54)+struct.pack('<IiiHHIIiiII',40,8,8,1,24,0,len(raw),0,0,0,0)+raw
    dib=struct.pack('<IiiHHIIiiII',40,8,16,1,32,0,256,0,0,0,0)+b'\x99\x88\x77\xff'*64+bytes(32)
    ico=struct.pack('<HHH',0,1,1)+struct.pack('<BBBBHHII',8,8,0,0,1,32,len(dib),22)+dib
    stream=io.BytesIO();Image.new('RGB',(8,8),(128,128,128)).save(stream,format='JPEG',quality=95,subsampling=0)
    return [(b'IMGTST  PNG',png_bytes),(b'IMGTST  BMP',bmp),(b'IMGTST  JPG',stream.getvalue()),(b'IMGTST  ICO',ico)]

def fixture(source):
    disk=Fat16(source);result=bytearray(disk.replace_file((BUILD/'image_controls_guest.x').read_bytes(),b'ICESTAR    ',b'COLOR   X  '))
    bpb=disk.partition_lba*512;fat_start=bpb+word(source,bpb+14)*512;fat_bytes=word(source,bpb+22)*512
    root_start=fat_start+source[bpb+16]*fat_bytes
    require(not any(disk.root[4*32:9*32]),'root resource slots unavailable')
    free=[c for c in range(2,(disk.end-disk.data_start)//disk.cluster_bytes+2)
        if all(word(f,c*2)==0 for f in disk.fat_copies)]
    clusters=[]
    for i,(name,content) in enumerate(resources()):
        require(len(content)<=disk.cluster_bytes,'resource exceeds one cluster')
        c=free[i];clusters.append(c)
        for copy in range(source[bpb+16]):struct.pack_into('<H',result,fat_start+copy*fat_bytes+c*2,0xffff)
        record=bytearray(32);record[:11]=name;record[11]=0x20
        struct.pack_into('<HI',record,26,c,len(content));result[root_start+(4+i)*32:root_start+(5+i)*32]=record
        at=disk.data_start+(c-2)*disk.cluster_bytes;result[at:at+len(content)]=content
    return bytes(result),bytes(result),clusters

def main():
    expected=(0x494d4731,*([1]*6),*([1]*12),0,1,*([1]*12))
    results=[]
    for label,path in [('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),('recovered',BUILD/'OpenIceStarOS_c_apps.vhd')]:
        results.append(boot(label,path,fixture_builder=fixture,suite='image_controls',entries=ENTRIES,
            expected_report=expected,compact_changes=True,instruction_trace=True))
    require(results[0]['report']==results[1]['report'],'image-control guest reports differ')
    summary=dict(cases=results,installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),
        resources={name.decode():hashlib.sha256(data).hexdigest() for name,data in resources()},
        checks=['real FAT reads and all four codecs','independent constant-color pixels (JPEG tolerance 1)',
            'actual owner-tagged allocation, full-screen mode and child compaction','all four entry/body traces','complete disposable disks unchanged'],
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (Path(__file__),ROOT/'tests/image_controls_guest.c')},complete_os_recovery=False)
    (BUILD/'image_controls_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('Four image-control bodies execute actual FAT/codec/pixel/heap chains; independent report and whole disks match.')
if __name__=='__main__':main()
