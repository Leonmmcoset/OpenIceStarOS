"""Original/C glyph and kerning calls with independent SFNT/fixture results."""
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
from tests.kernel_font_glyph import SLOTS,real_info
from tests.kernel_font_cmap import independent_map
ENTRIES=[(n,s,e) for n,_,s,e in SLOTS]

def fixture(source):
    data=Fat16(source).replace_file((BUILD/'font_glyph_guest.x').read_bytes(),
        b'ICESTAR    ',b'COLOR   X  ')
    return data,data,[]

def expected(source):
    font=Fat16(source).read_file(b'ICESTAR    ',b'FONT       ',b'FIRA    TTF')
    info,tables,count,fmt,selected=real_info(font)
    offsets=[struct.unpack_from('>H' if fmt==0 else '>I',font,tables[b'loca']+i*(2 if fmt==0 else 4))[0]*(2 if fmt==0 else 1)
             for i in range(count+1)]
    def box(glyph):
        offset=tables[b'glyf']+offsets[glyph]
        empty=offsets[glyph]==offsets[glyph+1]
        values=list(struct.unpack_from('>4h',font,offset+2)) if not empty else [0xa5a5a5a5]*4
        return [int(not empty),*values]
    values=[0x474c5931,1,1]
    for glyph in (0,1,7,13,34,113,138,264):
        offset=tables[b'glyf']+offsets[glyph]
        empty=offsets[glyph]==offsets[glyph+1]
        contours=struct.unpack_from('>h',font,offset)[0] if not empty else 0
        values += [-1 if empty else offset,*box(glyph),int(empty or contours==0)]
    mapping=independent_map(font,selected);glyph=mapping.get(65,0)
    values += box(glyph)
    hcount=struct.unpack_from('>H',font,tables[b'hhea']+34)[0]
    advance=struct.unpack_from('>h',font,tables[b'hmtx']+4*min(glyph,hcount-1))[0]
    bearing=struct.unpack_from('>h',font,tables[b'hmtx']+(4*glyph+2 if glyph<hcount else 4*hcount+2*(glyph-hcount)))[0]
    values += [advance,bearing]
    vertex=bytearray([0xa5]*16);vertex[12]=0x23
    struct.pack_into('<4H',vertex,0,0x7fff,65535,0x5678,65535)
    values += list(struct.unpack('<4I',vertex))
    # The specified rectangle emits move, three lines, and a closing line.
    values += [5,0,0,100,100,1,0,0,100,100,0]
    values += [2,1,1,2,-70,100,-70,-70,33]
    values += [0,-90,-90,-90]
    values += [1,1,-120,-120,-120,9]
    return tuple(v&0xffffffff for v in values)

def main():
    results=[]
    for label,path in [('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),
                       ('recovered',BUILD/'OpenIceStarOS_c_apps.vhd')]:
        results.append(boot(label,path,fixture_builder=fixture,suite='font_glyph',
            entries=ENTRIES,expected_report=expected(path.read_bytes()),compact_changes=True,
            instruction_trace=True))
    require(results[0]['report']==results[1]['report'],'glyph guest reports differ')
    summary=dict(cases=results,installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
                (Path(__file__),ROOT/'tests/font_glyph_guest.c',ROOT/'tests/kernel_font_glyph.py')},
        complete_os_recovery=False)
    (BUILD/'font_glyph_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('All fifteen glyph/kerning entries match independent reports; full disks unchanged.')

if __name__=='__main__':main()
