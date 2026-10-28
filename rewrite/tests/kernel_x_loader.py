"""Compare X read/allocation/relocation/launch to original x86, including corpus.

Filesystem, allocator, fill, cursor, dialog and task-creation boundaries are
deterministic. Production boot separately executes those real subsystems.
The complete affected byte ranges, boundary order/arguments and cdecl ABI
are checked for the original, ordinary linked C, and fixed-address C.
"""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
sys.path[:0] = [str(ROOT), str(BUILD / "test-deps")]
from tools.kernel_image import Fat16, ORIGINAL_SHA256
from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
    UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP,
    UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EFLAGS)

SLOTS = [
    ("x_load_relative", "kernel_x_load_relative_8020a6", 0x8020a6, 0x80214c),
    ("x_load_path", "kernel_x_load_path_80214c", 0x80214c, 0x802282),
    ("x_launch", "kernel_startup_launch_image", 0x802282, 0x80252a),
]
HELPERS = {
    0x825c8e:("kernel_x_find_relative_825c8e",1),
    0x827826:("kernel_x_header_relative_827826",2),
    0x826ade:("kernel_x_read_relative_826ade",2),
    0x8263c0:("kernel_x_find_path_8263c0",2),
    0x8278f4:("kernel_x_header_path_8278f4",2),
    0x82760c:("kernel_x_read_path_82760c",2),
    0x815464:("kernel_allocate",1),
    0x815728:("kernel_allocate_aligned",3),
    0x815b02:("kernel_fill_bytes",3),
    0x8131b0:("kernel_cursor_select",1),
    0x814688:("kernel_create_task",7),
    0x814a56:("kernel_create_application_task",7),
    0x811a42:("kernel_x_dialog_811a42",3),
    0x8082c6:("kernel_x_show_dialog_8082c6",1),
}
FLAGS = ["-m32","-march=i386","-Os","-ffreestanding","-fno-pic","-fno-pie",
    "-fno-stack-protector","-fno-asynchronous-unwind-tables","-ffunction-sections",
    "-fdata-sections","-fno-optimize-sibling-calls","-fno-delete-null-pointer-checks",
    "-mpreferred-stack-boundary=2","-Wall","-Wextra","-Werror"]
ARENA, ARENA_BYTES, STACK, STOP = 0x600000, 0x200000, 0x40f000, 0x200000
NAME, ALLOCATION, IMAGE = ARENA+0x100, ARENA+0x2000, ARENA+0x3000
META = bytes((i*17+9)&255 for i in range(32))


def run(command, **kwargs):
    return subprocess.run(command,cwd=ROOT,check=True,**kwargs)


def compile_variants(original, installed):
    obj = BUILD / "kernel_x_loader_preview.o"
    run(["gcc",*FLAGS,"-c","kernel/x_loader.c","-o",str(obj)])
    outputs = []
    for fixed in (False,True):
        label = "fixed" if fixed else "ordinary"
        script,elf = BUILD/f"x_loader_{label}.ld", BUILD/f"x_loader_{label}.elf"
        sections = "\n".join(
            f" .{section} {start:#x} : {{ *(.text.{symbol}) }}\n"
            f" ASSERT({symbol} == {start:#x} && SIZEOF(.{section}) <= {end-start}, \"slot overflow\")"
            for section,symbol,start,end in SLOTS) if fixed else ". = 0x300000; .text : { *(.text*) }"
        script.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_startup_launch_image)\n"+
            "\n".join(f"PROVIDE({symbol} = {address:#x});" for address,(symbol,_) in HELPERS.items())+
            "\nSECTIONS {\n"+sections+"\n /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n"
            " .unexpected : { *(*) }\n ASSERT(SIZEOF(.unexpected)==0, \"unexpected section\")\n}\n")
        run(["ld","-m","elf_i386","-T",str(script),"-o",str(elf),str(obj)])
        symbols={line.split()[-1]:int(line.split()[0],16)
                 for line in run(["nm",str(elf)],capture_output=True,text=True).stdout.splitlines()}
        if fixed:
            candidate = bytearray(original)
            sizes = {}
            for section,symbol,start,end in SLOTS:
                path = BUILD/f"x_loader_preview.{section}.bin"
                run(["objcopy","-O","binary","-j","."+section,str(elf),str(path)])
                body=path.read_bytes()
                assert 0<len(body)<=end-start
                candidate[start-0x800000:end-0x800000]=body+b"\x90"*(end-start-len(body))
                sizes[section]=dict(compiled_bytes=len(body),slot_bytes=end-start)
            if installed:
                production=(BUILD/"openicestaros_kernel.x").read_bytes()
                for _,_,start,end in SLOTS:
                    assert production[start-0x800000:end-0x800000]==candidate[start-0x800000:end-0x800000], "installed bytes differ"
                candidate=bytearray(production)
            path=BUILD/"x_loader_candidate.x"
            path.write_bytes(candidate)
            outputs.append((label,bytes(candidate),0x800000,[symbols[s] for _,s,_,_ in SLOTS]))
        else:
            path=BUILD/"x_loader_ordinary.bin"
            run(["objcopy","-O","binary",str(elf),str(path)])
            outputs.append((label,path.read_bytes(),0x300000,[symbols[s] for _,s,_,_ in SLOTS]))
    return outputs,sizes


class Machine:
    def __init__(self,label,binary,base,entries):
        self.label,self.entries=label,entries
        self.uc=Uc(UC_ARCH_X86,UC_MODE_32)
        self.uc.mem_map(0,0x1000000)
        self.uc.mem_write(base,binary)
        self.uc.hook_add(UC_HOOK_CODE,self.finished,begin=STOP,end=STOP)
        for address in HELPERS:
            self.uc.hook_add(UC_HOOK_CODE,self.boundary,begin=address,end=address)

    def u32(self,address):
        return struct.unpack("<I",self.uc.mem_read(address&0xffffffff,4))[0]

    def put32(self,address,value):
        self.uc.mem_write(address&0xffffffff,struct.pack("<I",value&0xffffffff))

    def finished(self,uc,address,size,unused):
        self.done=True
        uc.emu_stop()

    def boundary(self,uc,address,size,unused):
        label,count=HELPERS[address]
        stack=uc.reg_read(UC_X86_REG_ESP)
        args=list(struct.unpack(f"<{count}I",uc.mem_read(stack+4,count*4)))
        normalized=args.copy()
        if label=="kernel_x_find_path_8263c0":
            normalized[1]="metadata"
            self.record=args[1]
        elif label in ("kernel_x_header_path_8278f4","kernel_x_read_path_82760c"):
            assert args[0]==self.record,"metadata pointer changed"
            normalized[0]="metadata"
            normalized.append(bytes(uc.mem_read(args[0],32)).hex())
        # Include complete live image header and shared header at every boundary.
        self.trace.append([label,normalized,bytes(uc.mem_read(self.image,64)).hex(),
                           bytes(uc.mem_read(0x18a00,64)).hex()])
        result=(0x73410000+len(self.trace)*0x101)&0xffffffff
        case=self.case
        if label.startswith("kernel_x_find_"):
            result=case.get("lookup",1)
            if label=="kernel_x_find_path_8263c0":
                uc.mem_write(args[1],META)
        elif label.startswith("kernel_x_header_"):
            uc.mem_write(args[1],case["header"])
        elif label in ("kernel_allocate","kernel_allocate_aligned"):
            result=case.get("allocation",ALLOCATION)
            if case.get("allocation_mutation"):
                self.put32(0x18a04,0x4242)
                self.put32(0x18a0c,3)
        elif label=="kernel_fill_bytes":
            dest,value,length=args
            if length<=ARENA_BYTES and (dest+length<=0x20000 or ARENA<=dest<=ARENA+ARENA_BYTES-length):
                uc.mem_write(dest,bytes([value&255])*length)
            else:
                # Argument-fidelity probe only: unchecked enormous/wrapped fills
                # are not executed by this deterministic boundary.
                assert case.get("unsafe_fill_probe"),f"unsafe fill {args!r}"
            if case.get("fill_mutation"):
                self.put32(self.image+16,case["fill_mutation"])
            if case.get("metadata_mutation") and self.record:
                uc.mem_write(self.record,bytes(reversed(META)))
        elif label.startswith("kernel_x_read_"):
            uc.mem_write(args[1],case.get("payload",b"RECOVERED-X"))
        elif label=="kernel_cursor_select":
            if args[0]==1 and case.get("cursor_mutation"):
                self.put32(self.image,case["cursor_mutation"])
        elif label=="kernel_x_dialog_811a42":
            result=case.get("dialog",0xfedcba98)
        elif label in ("kernel_create_task","kernel_create_application_task"):
            result=case.get("task_result",0xffffffff)
            if case.get("task_mutation"):
                self.put32(self.image+28,case["task_mutation"])
        # Model caller-saved clobbers to expose bad argument/register retention.
        uc.reg_write(UC_X86_REG_EAX,result&0xffffffff)
        uc.reg_write(UC_X86_REG_ECX,0x11112222)
        uc.reg_write(UC_X86_REG_EDX,0x33334444)
        uc.reg_write(UC_X86_REG_ESP,stack+4)
        uc.reg_write(UC_X86_REG_EIP,self.u32(stack))

    def execute(self,case):
        self.case,self.trace,self.done,self.record=case,[],False,0
        uc=self.uc
        self.image=case.get("image",IMAGE)
        uc.mem_write(0,bytes(0x20000))
        uc.mem_write(ARENA,bytes([case.get("pattern",0xa5)])*ARENA_BYTES)
        uc.mem_write(STACK-0x4000,bytes(0x5000))
        uc.mem_write(NAME,b"C:\\ICESTAR\\PROGRAM.X\0")
        if case["function"]==2:
            uc.mem_write(self.image,case["header"])
            for offset,value in case.get("writes",[]):
                self.put32(self.image+offset,value)
            if case.get("binary"):
                uc.mem_write(self.image,case["binary"])
        args=([NAME,0xabc123] if case["function"]!=2 else [self.image,NAME])
        uc.mem_write(STACK,struct.pack("<5I",STOP,*args,0x13579bdf,0x2468ace0))
        saved=[(UC_X86_REG_EBX,0x12345678),(UC_X86_REG_ESI,0x87654321),
               (UC_X86_REG_EDI,0xabcdef12),(UC_X86_REG_EBP,0x11223344)]
        for reg,value in [*saved,(UC_X86_REG_ESP,STACK),(UC_X86_REG_EFLAGS,0x202)]:
            uc.reg_write(reg,value)
        try:
            uc.emu_start(self.entries[case["function"]],STOP+1,count=1000000)
        except UcError as error:
            regs={name:hex(uc.reg_read(reg)) for name,reg in (("EAX",UC_X86_REG_EAX),("EDX",UC_X86_REG_EDX),("ESP",UC_X86_REG_ESP))}
            raise AssertionError(f"{self.label} {case.get('name','case')} {case} at {uc.reg_read(UC_X86_REG_EIP):#x} {regs}: {error}") from error
        assert self.done,"execution did not return within instruction budget"
        assert uc.reg_read(UC_X86_REG_ESP)==STACK+4,"stack mismatch"
        assert bytes(uc.mem_read(STACK+4,16))==struct.pack("<4I",*args,0x13579bdf,0x2468ace0),"caller stack mutated"
        assert all(uc.reg_read(r)==v for r,v in saved),"callee-saved register mismatch"
        return (uc.reg_read(UC_X86_REG_EAX), self.trace,bytes(uc.mem_read(0,0x20000)),
                bytes(uc.mem_read(ARENA,ARENA_BYTES)))


def header(kind=0x0a,**overrides):
    fields=[kind,0x200,0,1,0x80,0,0,0,2,0x200,0,0x80,0,0,0,0]
    for key,value in overrides.items(): fields[int(key)]=value
    return struct.pack("<16I",*fields)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--installed",action="store_true")
    arguments=parser.parse_args()
    started=time.monotonic()
    BUILD.mkdir(exist_ok=True)
    disk=Fat16((ROOT.parent/"IceStarOS_x86_32M.vhd").read_bytes())
    original=disk.read_kernel()
    assert hashlib.sha256(original).hexdigest()==ORIGINAL_SHA256
    variants,sizes=compile_variants(original,arguments.installed)
    machines=[Machine("original",original,0x800000,[s[2] for s in SLOTS])]
    machines += [Machine(*v) for v in variants]
    cases=[]
    for function in (0,1):
        for kind in (0x0a,0x50,0,0xffffffff):
            for lookup in (0,1,2,0x7fffffff,0x80000000,0xffffffff):
                for allocation in (0,ALLOCATION,ALLOCATION+1):
                    cases.append(dict(function=function,header=header(kind),lookup=lookup,
                                      allocation=allocation))
        for kind in (0x0a,0x50):
            for size,pages,bss in ((0,0,0),(0xffffffff,1,1),(0x80000000,0x100000,0x80000000),
                                   (0xfffff000,1,0),(0xffffefff,1,0), (0x200,0xffffffff,0x80)):
                cases.append(dict(function=function,header=header(kind,**{"1":size,"3":pages,"11":bss}),
                                  unsafe_fill_probe=True))
            for mutation in ("allocation_mutation","metadata_mutation"):
                cases.append(dict(function=function,header=header(kind),**{mutation:True}))
    for kind in (0x0a,0x50,0,0x51,0xffffffff):
        for base in (IMAGE,IMAGE+1):
            for loaded in (0,1,0xffffffff):
                cases.append(dict(function=2,image=base,header=header(kind,**{"7":loaded}),
                    writes=[(0x200,0x100),(0x204,0x104),(0x100,0x1234),(0x104,0xfffffff0)]))
    for kind in (0x0a,0x50):
        for target in (4,16,28,0x100,0x104,0x200,0x204):
            writes=[(0x200,target),(0x100,0x1234),(0x104,0x1238),
                    (0x1234,0x10),(0x1238,0x14)]
            cases.append(dict(function=2,header=header(kind,**{"8":1}),writes=writes))
        for first,second in ((0x100,0x104),(0x104,0x100)):
            cases.append(dict(function=2,header=header(kind,**{"8":2}),
                writes=[(0x200,first),(0x204,second),(0x100,0x1234),(0x104,0x1238),
                        (0x1234,0x10),(0x1238,0x14)]))
        for mutation,value in (("cursor_mutation",0x50 if kind==0xa else 0xa),
                               ("cursor_mutation",0x51),("fill_mutation",0xffffffe0),
                               ("task_mutation",0xdeadbeef)):
            cases.append(dict(function=2,header=header(kind,**{"8":0}),**{mutation:value}))
    rng=random.Random(0x802282)
    for index in range(128):
        count=rng.randrange(3)
        targets=rng.sample((0x100,0x104),count)
        writes=[(0x400+i*4,target) for i,target in enumerate(targets)]
        writes.extend([(0x100,rng.choice((0x1234,0x1238))),
                       (0x104,rng.choice((0x1234,0x1238))),
                       (0x1234,0x10),(0x1238,0x14)])
        cases.append(dict(function=2,image=IMAGE+rng.randrange(4),
            header=header(rng.choice((0xa,0x50)),**{"8":count,"9":0x400,"3":rng.randrange(4)}),
            writes=writes,pattern=rng.randrange(256),task_result=rng.getrandbits(32)))
    corpus=[]
    inventory=json.loads((ROOT/"recovery/inventory.json").read_text())
    for program in inventory["programs"]:
        data=disk.read_file(*(p.encode("ascii") for p in program["short_names"]))
        if struct.unpack_from("<I",data)[0] not in (0xa,0x50): continue
        for base in (IMAGE,IMAGE+1):
            cases.append(dict(function=2,image=base,header=data[:64],binary=data,
                              name=program["path"]))
        corpus.append(dict(path=program["path"],sha256=hashlib.sha256(data).hexdigest()))
    for index,case in enumerate(cases):
        expected=machines[0].execute(case)
        for machine in machines[1:]:
            actual=machine.execute(case)
            if actual!=expected:
                for region,(a,b) in enumerate(zip(expected,actual)):
                    if a==b:continue
                    if isinstance(a,bytes):
                        offset=next(i for i,(x,y) in enumerate(zip(a,b)) if x!=y)
                        detail=f"memory {region} at {offset:#x}: {a[offset]:#x}!={b[offset]:#x}"
                    else:detail=f"region {region}: {a!r} != {b!r}"
                    raise AssertionError(f"{machine.label} case {index} {case}: {detail}")
    label="installed" if arguments.installed else "preview"
    paths=[ROOT/p for p in ("kernel/x_loader.c","kernel/x_loader.h","tests/kernel_x_loader.py")]
    paths += [BUILD/"x_loader_fixed.elf",BUILD/"x_loader_ordinary.elf",BUILD/"x_loader_candidate.x"]
    summary=dict(installed=arguments.installed,scenarios=len(cases),executions=len(cases)*len(machines),
        seconds=round(time.monotonic()-started,3),slots=sizes,corpus=corpus,
        compared="Return EAX, callee-saved registers, caller stack, ordered boundary arguments/live headers, full 128 KiB low memory and 2 MiB arena",
        limitations="External helpers are deterministic callbacks. Enormous/wrapped fills are argument-only probes. Unbounded relocation loops and invalid/unmapped targets are not covered.",
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths})
    (BUILD/f"x_loader_{label}_summary.json").write_text(json.dumps(summary,indent=2)+"\n")
    print(json.dumps(summary,indent=2))


if __name__=="__main__":
    main()
