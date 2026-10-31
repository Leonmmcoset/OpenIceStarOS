"""Exercise all recovered font parsers and original full font init in QEMU."""
import hashlib
import json
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT/'tools'),str(ROOT/'tests')]
from kernel_image import Fat16,require
from kernel_fat_mutation_qemu import boot

STARTS=[0x82b838,0x82b86e,0x82b8a2,0x82b8de,0x82b8fe,0x82b946,0x82b962,
    0x82b9ca,0x82ba70,0x82bb70,0x82bbe4,0x82bcc8,0x82bd28,0x82bd56,
    0x82be14,0x82be34,0x82be52,0x82be86,0x82beba,0x82bfe6,0x82c0b2,
    0x82c196,0x82c232,0x82c34c,0x82c3bc,0x82ca7c]
NAMES=['get8','peek8','seek','skip','getn','buffer','range','cff_index','cff_int',
    'cff_skip','cff_dict','cff_ints','cff_count','cff_item','u16','s16','u32',
    's32','signature','table','offset','count','cff_subrs','svg','original_full_init']
ENTRIES=list(zip(NAMES,STARTS,STARTS[1:]))


def fixture(source):
    result=Fat16(source).replace_file((BUILD/'font_parse_guest.x').read_bytes(),
        b'ICESTAR    ',b'COLOR   X  ')
    return result,result,[]


def report(source):
    font=Fat16(source).read_file(b'ICESTAR    ',b'FONT       ',b'FIRA    TTF')
    tables={}
    for i in range(struct.unpack_from('>H',font,4)[0]):
        tag,checksum,offset,size=struct.unpack_from('>4sIII',font,12+16*i)
        tables[tag]=offset
    glyphs=struct.unpack_from('>H',font,tables[b'maxp']+4)[0]
    return (0x464e5431,0x12,0x34,2,1,0x3456,1,1,1,11,108,3,1,108,
        (-1131)&0xffffffff,2,1,0x1234,(-2304)&0xffffffff,0x12345678,0xf700feff,
        1,1,1,0,1,tables[b'head'],0,0,1,glyphs,tables[b'loca'],tables[b'head'],
        tables[b'glyf'],tables[b'hhea'])


def main():
    results=[]
    for label,path in (('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),
                       ('recovered',BUILD/'OpenIceStarOS_c_apps.vhd')):
        results.append(boot(label,path,fixture_builder=fixture,suite='font_parse',
            entries=ENTRIES,expected_report=report(path.read_bytes()),compact_changes=True,
            instruction_trace=True))
    require(results[0]['report']==results[1]['report'],'font parse guest reports differ')
    require(all(r['changed_bytes']==0 for r in results),'font reader changed disk')
    summary=dict(cases=results,installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
            (Path(__file__),ROOT/'tests/font_parse_guest.c',ROOT/'tests/kernel_fat_mutation_qemu.py')},
        complete_os_recovery=False)
    (BUILD/'font_parse_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('All 24 font parsers and original full Fira initialization executed; independent report and unchanged disks verified.')


if __name__=='__main__':main()
