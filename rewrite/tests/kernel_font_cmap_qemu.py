"""Run cmap lookup in original/C guests with independent table expectations."""
import hashlib
import json
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(BUILD/'test-deps'),str(ROOT/'tools'),str(ROOT/'tests')]
from kernel_image import Fat16,require
from kernel_fat_mutation_qemu import boot
from tests.kernel_font_cmap import independent_map
POINTS=[0,1,32,65,66,68,70,71,86,97,255,256,1024,0x1234,0xfffd,
        65535,65536,65538,65540,0x1f600,0x10ffff,0xffffffff]
ENTRIES=[('cmap',0x82ca7c,0x82cede),('font_init',0x82c3bc,0x82ca7c)]

def fixture(source):
    data=Fat16(source).replace_file((BUILD/'font_cmap_guest.x').read_bytes(),
        b'ICESTAR    ',b'COLOR   X  ')
    return data,data,[]

def expected(source):
    font=Fat16(source).read_file(b'ICESTAR    ',b'FONT       ',b'FIRA    TTF')
    cmap=next(struct.unpack_from('>I',font,12+i*16+8)[0]
        for i in range(struct.unpack_from('>H',font,4)[0]) if font[12+i*16:16+i*16]==b'cmap')
    selected=None
    for i in range(struct.unpack_from('>H',font,cmap+2)[0]):
        platform,encoding,offset=struct.unpack_from('>HHI',font,cmap+4+i*8)
        if platform==0 or platform==3 and encoding in (1,10):selected=cmap+offset
    mapping=independent_map(font,selected)
    values=[0x434d5031,1,1]+[mapping.get(p,0) for p in POINTS]
    values += [(p*17+9)&255 if p<256 else 0 for p in POINTS[:-1]]
    values += [100+p-65 if 65<=p<69 else 0 for p in POINTS]
    values += [(p-3)&65535 if 65<=p<=68 else 700 if p==97 else 0 for p in POINTS]
    for kind in (12,13):
        for p in POINTS:
            match=next((r for r in [(65,70,100),(65536,65540,200),(0xffffffff,0xffffffff,65535)]
                        if r[0]<=p<=r[1]),None)
            values.append(match[2]+(p-match[0] if kind==12 else 0) if match else 0)
    values += [0]*len(POINTS)
    return tuple(values)

def main():
    results=[]
    for label,path in [('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),
                       ('recovered',BUILD/'OpenIceStarOS_c_apps.vhd')]:
        results.append(boot(label,path,fixture_builder=fixture,suite='font_cmap',
            entries=ENTRIES,expected_report=expected(path.read_bytes()),compact_changes=True,
            instruction_trace=True))
    require(results[0]['report']==results[1]['report'],'cmap guest reports differ')
    summary=dict(cases=results,installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
                (Path(__file__),ROOT/'tests/font_cmap_guest.c')},complete_os_recovery=False)
    (BUILD/'font_cmap_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('Original/C cmap lookups match Fira and all supported format fixtures; full disks unchanged.')

if __name__=='__main__':main()
