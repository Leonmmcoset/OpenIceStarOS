"""Run real nested-directory writes under original/C kernels on temporary disks.

Reuse the root writer's independently specified data/FAT expectations, move
the three affected records into a nested directory, and separately specify
the absolute writer's overwrite timestamp change and root forwarding result.
"""
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
from kernel_fat_write_qemu import fixture as root_fixture

ENTRIES = [
    ('write_path',0x8280e4,0x82867e),
    ('write_root',0x82867e,0x828b35),
    ('split_parent',0x828018,0x8280e4),
    ('find_directory_path',0x825f48,0x8263c0),
    ('read_alloc',0x827454,0x82760c),
    ('directory_find',0x825d34,0x825df4),
    ('release_record_chain',0x827e6c,0x827f58),
    ('free_allocation',0x815978,0x815a0a),
    ('write_sectors',0x827c72,0x827cc3),
    ('flush_metadata',0x827cc4,0x827d49),
    ('ata_write_sector',0x828c18,0x828c94),
]


def fixture(source):
    disk = Fat16(source)
    first,final,chains = root_fixture(source,guest='fat_write_path_guest.x')
    seeded,expected = bytearray(first),bytearray(final)
    bpb = disk.partition_lba*512
    fat_start = bpb+word(source,bpb+14)*512
    fat_bytes = word(source,bpb+22)*512
    root_start = fat_start+2*fat_bytes
    free = [c for c in range(2,(disk.end-disk.data_start)//8192+2)
            if word(disk.fat,c*2) == 0 and all(word(f,c*2) == 0 for f in disk.fat_copies)]
    require(len(free)>20,'directory fixture space unavailable')
    directories = [free[18],free[20]]
    filler = b'FILLER  TST'+bytes([0x20])+bytes(20)

    def fat_set(target,cluster,value):
        for copy in range(2):
            struct.pack_into('<H',target,fat_start+copy*fat_bytes+cluster*2,value)

    def cluster_start(cluster):
        return disk.data_start+(cluster-2)*8192

    def directory_record(name,cluster):
        entry = bytearray(32)
        entry[:11],entry[11] = name,0x10
        struct.pack_into('<H',entry,26,cluster)
        return entry

    # Three operations keep the root fixture's specified allocations/data:
    # new file, one-byte overwrite and zero-length creation. Here the old
    # record occupies index zero, and an overwrite must change its timestamp.
    inner_seed = seeded[root_start:root_start+8192]
    inner_expected = expected[root_start:root_start+8192]
    inner_seed[:32] = inner_seed[4*32:5*32]
    inner_expected[:32] = inner_expected[4*32:5*32]
    inner_seed[4*32:5*32] = inner_expected[4*32:5*32] = filler
    struct.pack_into('<HH',inner_expected,22,0x8edd,0x5836)
    # The deleted record is deliberately retained. The two available empty
    # slots take precedence; no scan beyond the allocated directory is needed.
    inner_expected[5*32:6*32] = inner_seed[5*32:6*32]

    # Undo the root fixture's fourth (deleted-record reuse) operation. Its
    # first candidate now belongs to the separate root-forwarding test.
    for c in chains['reuse']:
        offset = cluster_start(c)
        expected[offset:offset+8192] = seeded[offset:offset+8192]
        fat_set(expected,c,0)
    for c in (free[12],free[16]):
        for copy in range(2):
            offset = fat_start+copy*fat_bytes+c*2
            expected[offset:offset+2] = seeded[offset:offset+2]

    # Mount the untouched directory descriptors and the modified inner data.
    root_record = directory_record(b'WRDIR      ',directories[0])
    seeded[root_start+4*32:root_start+5*32] = root_record
    seeded[root_start+5*32:root_start+6*32] = bytes(32)
    expected[root_start:root_start+len(disk.root)] = seeded[root_start:root_start+len(disk.root)]
    outer = directory_record(b'SUB        ',directories[1])+bytes(8192-32)
    for target,inner in ((seeded,inner_seed),(expected,inner_expected)):
        for c in directories:
            require(word(first,fat_start+c*2) == 0,'directory cluster already occupied')
            fat_set(target,c,0xffff)
        offset = cluster_start(directories[0])
        target[offset:offset+8192] = outer
        offset = cluster_start(directories[1])
        target[offset:offset+8192] = inner

    root_cluster = free[5]
    fat_set(expected,root_cluster,0xffff)
    entry = bytearray(b'ROOTFOR TST'+bytes.fromhex('20 18 12 dc 8e 36 58 36 58 00 00 dd 8e 36 58'))
    entry += struct.pack('<HI',root_cluster,513)
    expected[root_start+5*32:root_start+6*32] = entry
    offset = cluster_start(root_cluster)
    expected[offset:offset+1024] = bytes((i*29+(i>>7)*3+211)&255 for i in range(1024))
    require(len(seeded) == len(source) == len(expected),'fixture resized disk')
    final_disk = Fat16(bytes(expected))
    for name,size in ((b'WRNEW   TST',8193),(b'WROLD   TST',1),(b'WRZERO  TST',0)):
        require(len(final_disk.read_file(b'WRDIR      ',b'SUB        ',name)) == size,
                'expected nested-file size')
    require(len(final_disk.read_file(b'ROOTFOR TST')) == 513,'expected root-forwarded size')
    return bytes(seeded),bytes(expected),dict(new=chains['new'],old=chains['old'],
        zero=chains['zero'],root_forward=[root_cluster],directories=directories,
        retained_deleted=[free[12],free[16]])


def main():
    report = (0x50415431,17,1,0,2,0,0,0,1,8193,1,1,1,0,1,513)
    results = [boot(label,path,fixture_builder=fixture,suite='fat_write_path',
                    entries=ENTRIES,expected_report=report,compact_changes=True)
        for label,path in (('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),
                           ('recovered',BUILD/'OpenIceStarOS_c_apps.vhd'))]
    for key in ('clusters','report','changed_bytes','changed_ranges'):
        require(results[0][key] == results[1][key],f'original/C absolute-write mismatch: {key}')
    summary = dict(cases=results,
        installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (Path(__file__),ROOT/'tests/fat_write_path_guest.c',
                      ROOT/'tests/kernel_fat_write_qemu.py',ROOT/'tests/kernel_fat_mutation_qemu.py')},
        complete_os_recovery=False)
    (BUILD/'fat_write_path_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('Original/C nested and root-forwarded writes match independent entire-disk images.')


if __name__ == '__main__':
    main()
