"""Real ATA reads under original/C kernels; independent hashes, unchanged disk."""
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT/'build'
sys.path[:0] = [str(ROOT/'tools'),str(ROOT/'tests')]
from kernel_image import Fat16,word,require
from kernel_fat_mutation_qemu import boot

ENTRIES = [
    ('x_lba_alloc',0x8252b4,0x82539e),
    ('x_lba_into',0x82539e,0x825476),
    ('sectors_alloc',0x825476,0x8254ca),
    ('read_sectors',0x8254ca,0x825506),
    ('read_partition',0x825506,0x8255ae),
    ('read_root_directory',0x8255ae,0x8256a8),
    ('alloc_root_directory',0x8256a8,0x8257bc),
    ('alloc_directory',0x8257bc,0x8258a6),
    ('ata_read_sector',0x828b44,0x828c18),
]


def fnv(payload):
    value=2166136261
    for byte in payload:
        value=((value^byte)*16777619)&0xffffffff
    return value


def fixture(source):
    disk=Fat16(source)
    result=bytearray(disk.replace_file((BUILD/'fat_block_guest.x').read_bytes(),
        b'ICESTAR    ',b'COLOR   X  '))
    bpb=disk.partition_lba*512
    require(word(source,bpb+11)==512 and source[bpb+13]==16,'fixture geometry changed')
    fat_start=bpb+word(source,bpb+14)*512
    fat_bytes=word(source,bpb+22)*512
    root_start=fat_start+2*fat_bytes
    require(not any(disk.root[4*32:6*32]),'fixture root slots unavailable')
    free=[c for c in range(2,(disk.end-disk.data_start)//disk.cluster_bytes+2)
        if word(disk.fat,c*2)==0 and all(word(f,c*2)==0 for f in disk.fat_copies)]
    require(len(free)>=2,'fixture clusters unavailable')
    directory_cluster,program_cluster=free[:2]
    directory_data=bytes((i*37+(i>>8)*13+91)&255 for i in range(8192))
    program_data=bytearray((i*23+(i>>7)*5+17)&255 for i in range(8192))
    struct.pack_into('<I',program_data,4,1025)
    for index,name,cluster,attributes,payload in (
        (4,b'RDBLOCK    ',directory_cluster,0x10,directory_data),
        (5,b'RDHEAD  TST',program_cluster,0x20,bytes(program_data))):
        record=bytearray(32)
        record[:11],record[11]=name,attributes
        struct.pack_into('<HI',record,26,cluster,1025 if index==5 else 0)
        result[root_start+index*32:root_start+(index+1)*32]=record
        for copy in range(2):
            struct.pack_into('<H',result,fat_start+copy*fat_bytes+cluster*2,0xffff)
        offset=disk.data_start+(cluster-2)*8192
        result[offset:offset+8192]=payload
    result=bytes(result)
    require(len(result)==len(source),'fixture resized disk')
    return result,result,[directory_cluster,program_cluster]


def report(source):
    seeded,_,clusters=fixture(source)
    disk=Fat16(seeded)
    head=disk.data_start+(clusters[1]-2)*8192
    folder=disk.data_start+(clusters[0]-2)*8192
    x_hash=fnv(seeded[head:head+1536])
    dir_hash=fnv(seeded[folder:folder+8192])
    return (0x424c4b31,1,x_hash,1,x_hash,1,x_hash,1,x_hash,1,
        fnv(seeded[17*512:18*512]),fnv(seeded[25*512:89*512]),
        1,dir_hash,1,dir_hash,1,dir_hash,0,0,0,1)


def main():
    results=[]
    for label,path in (('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),
                       ('recovered',BUILD/'OpenIceStarOS_c_apps.vhd')):
        results.append(boot(label,path,fixture_builder=fixture,suite='fat_block',
            entries=ENTRIES,expected_report=report(path.read_bytes()),compact_changes=True))
    require(results[0]['clusters']==results[1]['clusters'],'fixture allocation differs')
    # The C boot sector intentionally has different branding/code, while both
    # FATs/root metadata and the two read payloads must match between kernels.
    require(results[0]['report'][:10]+results[0]['report'][11:]==
            results[1]['report'][:10]+results[1]['report'][11:],'original/C payload mismatch')
    require(all(case['changed_bytes']==0 for case in results),'reader changed disk')
    summary=dict(cases=results,installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
            (Path(__file__),ROOT/'tests/fat_block_guest.c',ROOT/'tests/kernel_fat_mutation_qemu.py')},
        complete_os_recovery=False)
    (BUILD/'fat_block_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('All eight readers executed with real ATA; independent hashes and complete unchanged disks verified.')


if __name__=='__main__':
    main()
