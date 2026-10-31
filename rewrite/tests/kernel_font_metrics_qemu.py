"""Original/C metric and SVG calls with independent SFNT header expectations."""
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

ENTRIES=[
    ('hmetrics',0x82f958,0x82fa50),
    ('vmetrics',0x83054a,0x8305c0),
    ('typo_metrics',0x8305c0,0x83066a),
    ('bounds',0x83066a,0x8306ec),
    ('scale_height',0x8306ec,0x83073a),
    ('scale_em',0x83073a,0x830768),
    ('svg_record',0x83077c,0x83081c),
    ('glyph_svg',0x83081c,0x8308a0),
    ('codepoint_svg',0x8308a0,0x8308d2),
    ('full_font_init',0x82c3bc,0x82ca7c),
    ('cmap_lookup',0x82ca7c,0x82cede),
]


def fixture(source):
    result=Fat16(source).replace_file((BUILD/'font_metrics_guest.x').read_bytes(),
        b'ICESTAR    ',b'COLOR   X  ')
    return result,result,[]


def report(source):
    font=Fat16(source).read_file(b'ICESTAR    ',b'FONT       ',b'FIRA    TTF')
    tables={}
    for i in range(struct.unpack_from('>H',font,4)[0]):
        tag,checksum,offset,length=struct.unpack_from('>4sIII',font,12+i*16)
        tables[tag]=offset
    vertical=struct.unpack_from('>hhh',font,tables[b'hhea']+4)
    typo=struct.unpack_from('>hhh',font,tables[b'OS/2']+68)
    bounds=struct.unpack_from('>hhhh',font,tables[b'head']+36)
    horizontal=struct.unpack_from('>hh',font,tables[b'hmtx']+4)
    units=struct.unpack_from('>H',font,tables[b'head']+18)[0]
    bits=lambda value:struct.unpack('<I',struct.pack('<f',value))[0]
    values=(0x4d455431,1,1,*vertical,1,*typo,*bounds,*horizontal,
        bits(24/(vertical[0]-vertical[1])),bits(24/units),1,10,1,20,1,0,1)
    return tuple(value&0xffffffff for value in values)


def main():
    results=[]
    for label,path in (('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),
                       ('recovered',BUILD/'OpenIceStarOS_c_apps.vhd')):
        results.append(boot(label,path,fixture_builder=fixture,suite='font_metrics',
            entries=ENTRIES,expected_report=report(path.read_bytes()),compact_changes=True,
            instruction_trace=True))
    require(results[0]['report']==results[1]['report'],'metric guest reports differ')
    require(all(r['changed_bytes']==0 for r in results),'font read changed disk')
    summary=dict(cases=results,installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
            (Path(__file__),ROOT/'tests/font_metrics_guest.c',ROOT/'tests/kernel_fat_mutation_qemu.py')},
        complete_os_recovery=False)
    (BUILD/'font_metrics_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('Nine metric/scaling/SVG entries match independent expectations; full disks unchanged.')


if __name__=='__main__':main()
