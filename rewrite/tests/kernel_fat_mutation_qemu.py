"""Run real FAT release/delete/writeback on disposable QEMU disks.

Both the original kernel and the source-built kernel run the same diagnostic
X via CMD/PS2. Two synthetic root files use four otherwise free clusters.
Compare the complete resulting disk against independent expected changes.
The input images are read only by this script; QEMU receives temporary copies.
"""
import hashlib
import json
from pathlib import Path
import re
import socket
import struct
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'
sys.path[:0] = [str(ROOT/'tools')]
from kernel_image import Fat16, word, require
from boot_smoke import check_inputs, png_from_ppm

ENTRIES = [
    ('write_sectors', 0x827c72, 0x827cc3),
    ('flush_metadata', 0x827cc4, 0x827d49),
    ('release_root_chain', 0x827d4a, 0x827e6c),
    ('release_record_chain', 0x827e6c, 0x827f58),
    ('delete_root', 0x827f58, 0x827fe2),
    ('delete_relative', 0x827fe2, 0x828018),
    ('ata_write_sector', 0x828c18, 0x828c94),
]


def fixture(source):
    disk = Fat16(source)
    result = bytearray(disk.replace_file((BUILD/'fat_mutation_guest.x').read_bytes(),
                                        b'ICESTAR    ', b'COLOR   X  '))
    bpb = disk.partition_lba * 512
    fat_start = bpb + word(source, bpb+14) * 512
    fat_bytes = word(source, bpb+22) * 512
    root_start = fat_start + 2 * fat_bytes
    require(word(source,bpb+11) == 512 and source[bpb+16] == 2 and fat_bytes == 8192,
            'fixture geometry changed')
    require(disk.root[4*32] == 0 and not any(disk.root[4*32:7*32]),
            'disposable root slots unavailable')
    free = [c for c in range(2, min(len(disk.fat)//2,
        (disk.end-disk.data_start)//disk.cluster_bytes+2))
        if word(disk.fat,c*2) == 0 and all(word(f,c*2) == 0 for f in disk.fat_copies)]
    clusters = free[::2][:4]
    require(len(clusters) == 4, 'not enough common free clusters')
    for file_number, name in enumerate((b'MUTROOT TST', b'MUTREC  TST')):
        first, last = clusters[file_number*2:file_number*2+2]
        for copy in range(2):
            struct.pack_into('<H',result,fat_start+copy*fat_bytes+first*2,last)
            struct.pack_into('<H',result,fat_start+copy*fat_bytes+last*2,0xffff)
        record = bytearray(32)
        record[:11], record[11] = name, 0x20
        struct.pack_into('<H',record,26,first)
        struct.pack_into('<I',record,28,2*disk.cluster_bytes)
        result[root_start+(4+file_number)*32:root_start+(5+file_number)*32] = record
        for c in (first,last):
            offset = disk.data_start+(c-2)*disk.cluster_bytes
            result[offset:offset+disk.cluster_bytes] = bytes([0x31+file_number])*disk.cluster_bytes
    expected = bytearray(result)
    for copy in range(2):
        for c in clusters:
            struct.pack_into('<H',expected,fat_start+copy*fat_bytes+c*2,0)
    for index in (4,5):
        expected[root_start+index*32] = 0xe5
    return bytes(result), bytes(expected), clusters


def boot(label, source_path, *, fixture_builder=fixture, suite='fat_mutation',
         entries=ENTRIES, expected_report=(0x4d555431,4,5,2,1,2,1,0xffffffff,
                                          0xffffffff,64,0xe5e5,1),
         compact_changes=False, instruction_trace=False):
    source = source_path.read_bytes()
    seeded, expected, clusters = fixture_builder(source)
    output = BUILD/f'{suite}_qemu_{label}'
    output.mkdir(exist_ok=True)
    debug, trace = output/'debug.bin', output/'execution.trace'
    for p in (debug,trace):
        p.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix='openicestaros-fat-write-') as temp:
        directory = Path(temp)
        image, monitor = directory/'disposable.vhd', directory/'monitor.sock'
        image.write_bytes(seeded)
        require(image.resolve() != source_path.resolve(), 'refusing source image as write target')
        with (output/'qemu.log').open('wb') as log:
            process = subprocess.Popen(['qemu-system-i386','-machine','pc','-m','128',
                '-drive',f'file={image},format=raw','-boot','c','-display','none','-vga','std',
                '-no-reboot','-serial','none','-monitor',f'unix:{monitor},server=on,wait=off',
                '-debugcon',f'file:{debug}','-global','isa-debugcon.iobase=0xe9',
                '-dfilter',','.join(f'{start:#x}..{end:#x}' for _,start,end in entries),
                '-D',str(trace)],
                stdout=log,stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic()+10
                while not monitor.exists():
                    require(process.poll() is None and time.monotonic()<deadline,'QEMU monitor unavailable')
                    time.sleep(0.05)
                with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as connection:
                    connection.settimeout(10)
                    connection.connect(str(monitor))

                    def prompt():
                        data = b''
                        while not data.endswith(b'(qemu) '):
                            block = connection.recv(65536)
                            require(bool(block),'QEMU monitor closed')
                            data += block
                        return data

                    def execute(line):
                        connection.sendall(line.encode()+b'\n')
                        return prompt()

                    prompt()
                    time.sleep(12)
                    cmd_base = check_inputs(execute,output,image)
                    execute('stop')
                    memory = output/'installed_slots.memory'
                    execute(f'pmemsave 0x800000 0x{len(Fat16(seeded).read_kernel()):x} "{memory}"')
                    loaded = memory.read_bytes()
                    kernel = Fat16(seeded).read_kernel()
                    for _,start,end in entries:
                        require(loaded[start-0x800000:end-0x800000] ==
                                kernel[start-0x800000:end-0x800000], 'guest kernel interval mismatch')
                    execute(f'pmemsave 0x1a83a 0x8 "{output}/fat_pointers.memory"')
                    fat_pointer = struct.unpack('<I',(output/'fat_pointers.memory').read_bytes()[:4])[0]
                    disk = Fat16(seeded)
                    bpb = disk.partition_lba*512
                    metadata_start = bpb+word(seeded,bpb+14)*512
                    metadata_bytes = seeded[bpb+16]*word(seeded,bpb+22)*512+len(disk.root)
                    execute(f'pmemsave {fat_pointer:#x} {metadata_bytes:#x} "{output}/metadata_before.memory"')
                    require((output/'metadata_before.memory').read_bytes() ==
                            seeded[metadata_start:metadata_start+metadata_bytes],
                            'cached FAT/root fixture differs before diagnostic')
                    execute('log exec,nochain,in_asm' if instruction_trace else 'log exec,nochain')
                    execute('cont')
                    for line in ('cd icestar','color.x'):
                        for char in line:
                            execute(f"sendkey { {' ': 'spc', '.': 'dot'}.get(char,char)} 30")
                            time.sleep(0.08)
                        time.sleep(0.2)
                        execute('stop')
                        scratch = output/'command.memory'
                        execute(f'pmemsave {cmd_base+0xb2da:#x} {len(line)+1:#x} "{scratch}"')
                        require(scratch.read_bytes() == line.encode()+b'\0','CMD diagnostic input mismatch')
                        execute('cont')
                        execute('sendkey ret 30')
                        time.sleep(1)
                    deadline = time.monotonic()+15
                    while debug.stat().st_size < len(expected_report)*4:
                        require(process.poll() is None and time.monotonic()<deadline,'guest write test timed out')
                        time.sleep(0.1)
                    execute('stop')
                    execute('log none')
                    execute(f'screendump "{output}/completed.ppm"')
                    png_from_ppm(output/'completed.ppm',output/'completed.png')
                    # Closing the raw backend drains host writes; poll-only
                    # guest remains stopped, so no further mutations can occur.
                    connection.sendall(b'quit\n')
                    while connection.recv(65536):
                        pass
                process.wait(timeout=10)
                require(process.returncode == 0,'QEMU exited with an error')
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait()
        actual = image.read_bytes()
    report = struct.unpack(f'<{len(expected_report)}I',debug.read_bytes())
    require(report == expected_report,
            f'guest result mismatch: {report}')
    if actual != expected:
        (output/'unexpected.vhd').write_bytes(actual)
        (output/'expected.vhd').write_bytes(expected)
        offsets = [i for i,(a,b) in enumerate(zip(actual,expected)) if a != b]
        raise AssertionError(f'{label}: unexpected disk bytes: {offsets[:16]}')
    trace_text = trace.read_text()
    pcs = [int(m.group(1),16) for m in re.finditer(r'\[[0-9a-fA-F]+/([0-9a-fA-F]+)/',trace_text)]
    decoded = [int(m.group(1),16) for m in re.finditer(r'^0x([0-9a-fA-F]+):',trace_text,re.MULTILINE)]
    functions = {}
    for name,start,end in entries:
        hits = pcs.count(start)
        body_events = sum(start < pc < end for pc in pcs)
        body_instructions = sum(start < pc < end for pc in decoded)
        # A straight-line leaf can fit wholly in its entry translation block.
        # For those leaves, entry execution plus that block's instruction
        # listing and the independent guest report establish body execution.
        require(hits > 0 and (body_events or (instruction_trace and body_instructions)),
                f'entry/body not executed: {name}')
        functions[name] = dict(entry=hex(start),entry_events=hits)
        if instruction_trace:
            functions[name].update(interior_block_events=body_events,
                                   decoded_body_instructions=body_instructions)
    changed = [i for i,(a,b) in enumerate(zip(seeded,actual)) if a != b]
    require(source_path.read_bytes() == source,'input image was modified')
    evidence = dict(label=label,diagnostic_only=True,source_sha256=hashlib.sha256(source).hexdigest(),
        seeded_sha256=hashlib.sha256(seeded).hexdigest(),result_sha256=hashlib.sha256(actual).hexdigest(),
        clusters=clusters,report=list(report),functions=functions,
        disk_bytes_compared=len(actual),input='CMD via emulated PS/2; no guest-state injection',
        scope='Fixture operations; complete disk equality. Temporary writable image only.')
    if compact_changes:
        ranges = []
        for offset in changed:
            if ranges and offset == ranges[-1][1]:
                ranges[-1][1] += 1
            else:
                ranges.append([offset,offset+1])
        evidence.update(changed_bytes=len(changed),changed_ranges=ranges)
    else:
        evidence['changed_offsets'] = changed
    (output/'summary.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print(json.dumps(evidence),flush=True)
    return evidence


def main():
    results = []
    for label,image in (('original',ROOT.parent/'IceStarOS_x86_32M.vhd'),
                        ('recovered',BUILD/'OpenIceStarOS_c_apps.vhd')):
        results.append(boot(label,image))
    require(results[0]['clusters'] == results[1]['clusters'] and
            results[0]['report'] == results[1]['report'] and
            results[0]['changed_offsets'] == results[1]['changed_offsets'],
            'original/recovered writeback behavior differs')
    summary = dict(cases=results,installed_intervals=len(json.loads((ROOT/'kernel/patches.json').read_text())),
                   complete_os_recovery=False)
    (BUILD/'fat_mutation_qemu_summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('Original and recovered kernels agree: real FAT chain release, deletion and ATA writeback; entire disks verified.')


if __name__ == '__main__':
    main()
