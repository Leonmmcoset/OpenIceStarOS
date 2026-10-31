"""Compare real root-file creation/overwrite to independent whole-disk images.

Boot the original and installed C kernels with the same diagnostic X, using
the CMD/PS2 runner from the chain-deletion test. Only temporary disks are writable.
"""
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT/'build'
sys.path[:0] = [str(ROOT/'tools'), str(ROOT/'tests')]
from kernel_image import Fat16, word, require
from kernel_fat_mutation_qemu import boot

ENTRIES = [
    ('write_root',0x82867e,0x828b35),
    ('split_parent',0x828018,0x8280e4),
    ('write_sectors',0x827c72,0x827cc3),
    ('flush_metadata',0x827cc4,0x827d49),
    ('release_root_chain',0x827d4a,0x827e6c),
    ('find_free_cluster',0x827ace,0x827b44),
    ('find_free_after',0x827b44,0x827bba),
    ('ata_write_sector',0x828c18,0x828c94),
]


def fixture(source):
    disk = Fat16(source)
    seeded = bytearray(disk.replace_file((BUILD/'fat_write_guest.x').read_bytes(),
                                         b'ICESTAR    ', b'COLOR   X  '))
    bpb = disk.partition_lba*512
    fat_start = bpb + word(source,bpb+14)*512
    fat_bytes = word(source,bpb+22)*512
    root_start = fat_start+2*fat_bytes
    require(disk.cluster_bytes == 8192 and len(disk.root) == 16384 and fat_bytes == 8192,
            'write fixture geometry changed')
    require(not any(disk.root[4*32:]),'root fixture space unavailable')
    free = [c for c in range(2,(disk.end-disk.data_start)//disk.cluster_bytes+2)
            if word(disk.fat,c*2) == 0 and all(word(f,c*2) == 0 for f in disk.fat_copies)]
    require(len(free) >= 17 and free[:17] == list(range(free[0],free[0]+17)),
            'expected contiguous free fixture area')

    def fat_set(target,cluster,value):
        for copy in range(2):
            struct.pack_into('<H',target,fat_start+copy*fat_bytes+cluster*2,value)

    def record_set(target,index,record):
        target[root_start+index*32:root_start+(index+1)*32] = record

    def record(name,cluster,size):
        # Independently specified legacy attributes/date/time bytes.
        entry = bytearray(name + bytes.fromhex('20 18 12 dc 8e 36 58 36 58 00 00 dd 8e 36 58'))
        require(len(entry) == 26,'fixture record layout')
        return entry + struct.pack('<HI',cluster,size)

    def cluster_set(target,cluster,data):
        start = disk.data_start+(cluster-2)*disk.cluster_bytes
        target[start:start+len(data)] = data

    # The original image has five unused entries free only in FAT1. Allocation
    # scans FAT1 alone; reserve those explicitly in this disposable fixture so
    # the specified chains start in the common-free area selected above.
    for c in range(2,free[0]):
        if word(disk.fat,c*2) == 0:
            fat_set(seeded,c,0xfff7)

    # Fragment new allocations with two unavailable clusters; old/deleted
    # records each retain a distinct noncontiguous chain and recognizable data.
    for c in (free[1],free[6]):
        fat_set(seeded,c,0xfff7)
    for index,name,first,last,pattern in (
            (4,b'WROLD   TST',free[4],free[8],0x31),
            (5,b'WRDEL   TST',free[12],free[16],0x32)):
        entry = bytearray(name+bytes([0x21])+bytes([0x67])*14+struct.pack('<HI',first,16384))
        struct.pack_into('<H',entry,20,0)
        if index == 5: entry[0] = 0xe5
        record_set(seeded,index,entry)
        fat_set(seeded,first,last)
        fat_set(seeded,last,0xffff)
        for c in (first,last): cluster_set(seeded,c,bytes([pattern])*8192)
    # Leave precisely two zero records. Empty slots take priority over the
    # deleted slot; after new+zero creation, the deleted record must be reused.
    for index in range(8,512):
        record_set(seeded,index,b'FILLER  TST'+bytes([0x20])+bytes(20))

    expected = bytearray(seeded)
    chains = {'new':[free[0],free[2]], 'old':[free[4]],
              'zero':[free[3]], 'reuse':[free[5],free[7],free[8]]}
    for c in (free[8],free[12],free[16]): fat_set(expected,c,0)
    for chain in chains.values():
        for c,nxt in zip(chain,chain[1:]+[0xffff]): fat_set(expected,c,nxt)
    record_set(expected,6,record(b'WRNEW   TST',chains['new'][0],8193))
    # Root overwrite preserves all timestamps, attributes and starting cluster.
    struct.pack_into('<I',expected,root_start+4*32+28,1)
    record_set(expected,7,record(b'WRZERO  TST',chains['zero'][0],0))
    record_set(expected,5,record(b'WRREUSE TST',chains['reuse'][0],16385))
    for key,seed,length in (('new',17,16384),('old',83,512),('reuse',149,24576)):
        data = bytes((i*29+(i>>7)*3+seed)&255 for i in range(length))
        for position,c in enumerate(chains[key]):
            cluster_set(expected,c,data[position*8192:(position+1)*8192])
    # Check both declared content and the extra sectors written after EOF.
    final = Fat16(bytes(expected))
    for name,size in ((b'WRNEW   TST',8193),(b'WROLD   TST',1),
                      (b'WRZERO  TST',0),(b'WRREUSE TST',16385)):
        require(len(final.read_file(name)) == size,'expected fixture file size')
    return bytes(seeded),bytes(expected),chains


def main():
    expected_report = (0x57525431,1,1,1,17,1,0,33,6,4,7,5)
    results = [boot(label,path,fixture_builder=fixture,suite='fat_write',entries=ENTRIES,
                    expected_report=expected_report,compact_changes=True)
        for label,path in (('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),
                           ('recovered',BUILD/'OpenIceStarOS_c_apps.vhd'))]
    for key in ('clusters','report','changed_bytes','changed_ranges'):
        require(results[0][key] == results[1][key],f'original/C write mismatch: {key}')
    summary = dict(cases=results,
        installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()
                for p in (Path(__file__), ROOT/'tests/fat_write_guest.c',
                          ROOT/'tests/kernel_fat_mutation_qemu.py')},
        complete_os_recovery=False)
    (BUILD/'fat_write_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('Original/C root writes agree with independent whole-disk expectations.')


if __name__ == '__main__':
    main()
