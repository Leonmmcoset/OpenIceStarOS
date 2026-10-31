"""Real Fira, CFF, CID and partial initializer failures in original/C guests."""
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
from tests.kernel_font_glyph import real_info
ENTRIES=[('font_init',0x82c3bc,0x82ca7c),('cff_subrs',0x82c232,0x82c34c),
         ('cff_index',0x82b9ca,0x82ba70),('cff_dict',0x82bbe4,0x82bcc8)]

def fixture(source):
    data=Fat16(source).replace_file((BUILD/'font_init_guest.x').read_bytes(),
        b'ICESTAR    ',b'COLOR   X  ')
    return data,data,[]

def expected(source):
    font=Fat16(source).read_file(b'ICESTAR    ',b'FONT       ',b'FIRA    TTF')
    info,tables,count,fmt,selected=real_info(font)
    values=[0x494e4931,1,1,count,tables[b'loca'],tables[b'head'],tables[b'glyf'],tables[b'hhea'],
        tables[b'hmtx'],tables.get(b'kern',0),tables.get(b'GPOS',0),0xffffffff,selected,fmt,0,0,0]
    sentinel=0xa5a5a5a5
    for cid,kind,missing in [(0,2,0),(1,2,0),(0,1,0),(0,2,1)]:
        ok=kind==2 and not missing
        values += [int(ok),17 if ok else sentinel,0,0 if missing else 576,0,640,704,
                   0xffffffff if ok else sentinel,544 if ok else sentinel,1 if ok else sentinel]
        if missing:values += [0,0,0]+[sentinel]*15
        else:
            values += [1024,0,0x20000000]
            values += [1088,0,6] if ok else [sentinel]*3
            values += [1068,0,6,0,0,0]
            values += [1120,0,5,1144,0,0x20000000-120] if cid else [0]*6
    return tuple(values)

def main():
    results=[]
    for label,path in [('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),
                       ('recovered',BUILD/'OpenIceStarOS_c_apps.vhd')]:
        results.append(boot(label,path,fixture_builder=fixture,suite='font_init',
            entries=ENTRIES,expected_report=expected(path.read_bytes()),compact_changes=True,
            instruction_trace=True))
    require(results[0]['report']==results[1]['report'],'initializer guest reports differ')
    summary=dict(cases=results,installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
            (Path(__file__),ROOT/'tests/font_init_guest.c')},complete_os_recovery=False)
    (BUILD/'font_init_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('Fira/CFF/CID initialization and partial failures match; full disks unchanged.')

if __name__=='__main__':main()
