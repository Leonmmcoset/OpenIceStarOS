"""Installed glyph baking/range drawing with real original/recovered guests."""
import hashlib
import json
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(BUILD/'test-deps'),str(ROOT/'tools'),str(ROOT/'tests')]
from kernel_image import Fat16,require
from kernel_fat_mutation_qemu import boot
from tests.kernel_font_bake import SLOTS
ENTRIES=[(n,s,e) for n,_,s,e in SLOTS]
def fixture(source):
    data=Fat16(source).replace_file((BUILD/'font_bake_guest.x').read_bytes(),b'ICESTAR    ',b'COLOR   X  ')
    return data,data,[]
def words(data):return list(struct.unpack('<'+'I'*(len(data)//4),data))
def floats(*v):return words(struct.pack('<'+'f'*len(v),*v))
def signature(data):
    h=2166136261
    for b in data:h=((h^b)*16777619)&0xffffffff
    return h
def draw(data,start,width,height,stride):
    for y in range(height):data[start+y*stride:start+y*stride+width*4]=bytes([255])*width+bytes(width*3)
def expected():
    values=[0x42414b31,1,0,0xffffffff,1,*floats(12,-4,2,6,-2,1)]
    chars=b''.join(struct.pack('<4H3f',*v) for v in [(1,1,7,5,1,-4,10),(8,1,14,5,1,-4,12),(1,6,1,6,0,0,8)])
    pixels=bytearray(bytes(16*12*4)+b'\xa5'*(4096-16*12*4));draw(pixels,17,6,4,16);draw(pixels,24,6,4,16)
    for _ in range(2):values += [7,*words(chars),signature(pixels)]
    pixels=bytearray(b'\xa5'*4096)
    for y in range(4):pixels[y*32:y*32+32]=bytes([255])*6+bytes(26)
    values += [0,0,signature(pixels)]
    pixels=bytearray(b'\xa5'*4096);draw(pixels,3+4*32,6,4,32)
    chars=struct.pack('<4H5f',3,4,9,8,1,-4,12,7,0)
    values += [1,3,4,99,6,4,1,*words(chars),signature(pixels),3,4]
    # Outer 16x12, padding1 -> inner15x11: two 7x5 cells share row0;
    # the empty glyph retains one-pixel padding at (14,0).
    chars=b''.join(struct.pack('<4H5f',*v) for v in
        [(1,1,7,5,1,-4,10,7,0),(8,1,14,5,1,-4,12,7,0),(15,1,15,1,0,0,8,0,0)])
    pixels=bytearray(bytes(16*12*4)+b'\xa5'*(4096-16*12*4));draw(pixels,17,6,4,16);draw(pixels,24,6,4,16)
    for _ in range(2):values += [1,1,*words(chars),signature(pixels)]
    return tuple(values)
def main():
    results=[]
    for label,path in [('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),('recovered',BUILD/'OpenIceStarOS_c_apps.vhd')]:
        results.append(boot(label,path,fixture_builder=fixture,suite='font_bake',entries=ENTRIES,
            expected_report=expected(),compact_changes=True,instruction_trace=True))
    require(results[0]['report']==results[1]['report'],'bake/range reports differ')
    summary=dict(cases=results,installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
            (Path(__file__),ROOT/'tests/font_bake_guest.c')},complete_os_recovery=False)
    (BUILD/'font_bake_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('Ten installed bake/range APIs match independent metadata/pixels/metrics; whole disks unchanged.')
if __name__=='__main__':main()
