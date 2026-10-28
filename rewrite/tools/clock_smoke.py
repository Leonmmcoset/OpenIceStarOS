"""Load compiled CLOCK through CMD, observe its face/hands, then close it."""
import hashlib
import json
import math
from pathlib import Path
import struct
import time
from boot_smoke import png_from_ppm
from kernel_image import Fat16, require
from xformat import XImage


def check_clock(execute, build, image, cmd_base):
    evidence = {"image": str(image), "cmd_base": hex(cmd_base)}
    scratch = build / "clock_state.memory"

    def read_memory(address, size):
        execute("stop")
        try:
            scratch.unlink(missing_ok=True)
            execute(f'pmemsave {address:#x} {size:#x} "{scratch}"')
            data = scratch.read_bytes()
            require(len(data) == size, "incomplete CLOCK state read")
            return data
        finally:
            execute("cont")

    def dword(address):
        return struct.unpack("<I", read_memory(address, 4))[0]

    def position():
        return struct.unpack("<ii", read_memory(dword(0x1b048) + 12, 8))

    def move_to(x, y):
        for _ in range(30):
            px, py = position()
            dx, dy = x - px, y - py
            if abs(dx) <= 2 and abs(dy) <= 2:
                return px, py
            execute(f"mouse_move {max(-100, min(100, dx))} {max(-100, min(100, dy))}")
            time.sleep(0.12)
        raise RuntimeError(f"pointer did not reach {(x, y)}; observed {position()}")

    def capture(name):
        ppm = build / f"clock_{name}.ppm"
        execute("stop")
        try:
            execute(f'screendump "{ppm}"')
        finally:
            execute("cont")
        png_from_ppm(ppm, build / f"clock_{name}.png")
        data = ppm.read_bytes()
        _, dimensions, _, pixels = data.split(b"\n", 3)
        width, height = map(int, dimensions.split())
        return width, height, pixels

    def command(line):
        for character in line:
            key = {" ": "spc", ".": "dot"}.get(character, character)
            execute(f"sendkey {key} 30")
            time.sleep(0.08)
        time.sleep(0.2)
        require(read_memory(cmd_base + 0xb2da, len(line) + 1) == line.encode() + b"\0",
                "CMD input mismatch before executing " + line)
        execute("sendkey ret 30")
        time.sleep(0.8)

    command("cd icestar")
    require(read_memory(cmd_base+0xb1da,255).split(b"\0")[0].upper()==b"C:\\ICESTAR",
            "CMD did not enter CLOCK directory")
    command("clock.x")
    binary=Fat16(Path(image).read_bytes()).read_file(b"ICESTAR    ",b"CLOCK   X  ")
    require(binary==(build/"clock.x").read_bytes(),"disk does not contain compiled CLOCK")
    parsed=XImage(binary)
    symbols=json.loads((build/"clock.symbols.json").read_text())["symbols"]
    start=0x900000
    ram=read_memory(start,128*1024*1024-start)
    bases=[]; offset=ram.find(binary[:0x1c])
    while offset>=0:
        base=start+offset
        if struct.unpack_from("<I",ram,offset+0x1c)[0]==base:
            expected=parsed.load(base)
            require(ram[offset+0x40:offset+parsed.bss_start]==expected[0x40:parsed.bss_start],
                    "guest CLOCK differs from relocated disk code/data")
            bases.append(base)
        offset=ram.find(binary[:0x1c],offset+1)
    del ram
    require(len(bases)==1,"expected one running relocated CLOCK")
    base=bases[0]
    record,window,surface,unused=struct.unpack("<4I",read_memory(base+parsed.bss_start,16))
    require(window>0 and dword(0x6a0000+window*4)==record,"CLOCK pool/BSS mismatch")
    require(dword(record+0xc)==surface and unused==0,"CLOCK surface/BSS mismatch")
    close=dword(record+0x5b7)
    require(close==base+symbols["clock_close"]["address"],"CLOCK close callback mismatch")
    task=dword(record+4);task_address=dword(0x1b0c8)+task*108
    require(dword(task_address)!=0,"CLOCK task not active")
    wx,wy,ww,wh=struct.unpack("<4I",read_memory(record+0x12b,16))
    require((wx,wy,ww,wh)==(200,100,400,380),"CLOCK window geometry changed")
    palette=struct.unpack("<9I",read_memory(dword(0x1b0e0),36))
    fore=bytes((palette[1]>>16&255,palette[1]>>8&255,palette[1]&255))
    back=bytes((palette[8]>>16&255,palette[8]>>8&255,palette[8]&255))
    evidence.update(base=hex(base),binary_sha256=hashlib.sha256(binary).hexdigest(),
        relocations_verified=len(parsed.relocations),state=[hex(record),window,surface,unused],
        close_callback=hex(close),task=task,task_active_before_close=dword(task_address),
        geometry=[wx,wy,ww,wh],foreground=list(fore),background=list(back))
    def save():
        (build/"clock_smoke.json").write_text(json.dumps(evidence,indent=2)+"\n")
    samples=[]; signatures=set(); times=set()
    # Screenshots may catch an in-progress clear/redraw. Require three complete
    # samples, each with a rim and all twelve labels, rather than accepting a
    # changed hash alone as proof of a running clock.
    deadline=time.monotonic()+12
    while len(signatures)<3 and time.monotonic()<deadline:
        packed_time=dword(0x1b004)
        width,height,pixels=capture("sample")
        def pixel(x,y):
            return pixels[(y*width+x)*3:(y*width+x+1)*3]
        def count(x0,y0,x1,y1,color):
            return sum(pixel(x,y)==color for y in range(y0,y1) for x in range(x0,x1))
        rim=[]
        for dx,dy in ((150,0),(-150,0),(0,150),(0,-150)):
            x,y=wx+200+dx,wy+205+dy
            rim.append(count(x-4,y-4,x+5,y+5,fore))
        labels=[]
        for i in range(12):
            angle=i*math.pi/6
            x=wx+200+int(math.sin(angle)*120)
            y=wy+205-int(math.cos(angle)*120)
            labels.append(count(x-4,y-4,x+20,y+20,fore))
        center=count(wx+190,wy+195,wx+211,wy+216,fore)
        cleared=count(wx+20,wy+35,wx+40,wy+55,back)
        crop=b"".join(pixels[((wy+y)*width+wx+55)*3:((wy+y)*width+wx+345)*3]
                      for y in range(60,351))
        digest=hashlib.sha256(crop).hexdigest()
        complete=all(n>0 for n in rim) and all(n>5 for n in labels) and center>5 and cleared==400
        samples.append(dict(time=hex(packed_time),signature=digest,rim=rim,labels=labels,
                            center=center,background=cleared,complete=complete))
        if complete:
            if digest not in signatures:
                source=build/"clock_sample.ppm";target=build/f"clock_frame{len(signatures)}.ppm"
                target.write_bytes(source.read_bytes());png_from_ppm(target,target.with_suffix(".png"))
                signatures.add(digest);times.add(packed_time)
        time.sleep(0.45)
    evidence.update(samples=samples,complete_distinct_frames=len(signatures),distinct_times=len(times));save()
    require(len(signatures)==3 and len(times)>=2,"CLOCK face/hands did not produce three complete changing frames")
    evidence["pointer_close_button"]=move_to(wx+ww-13,wy+12)
    execute("mouse_button 1");time.sleep(0.1);execute("mouse_button 0");time.sleep(1)
    capture("closed")
    evidence.update(window_pool_after_close=hex(dword(0x6a0000+window*4)),
                    task_active_after_close=dword(task_address));save()
    require(evidence["window_pool_after_close"]=="0x0","CLOCK window was not removed")
    require(evidence["task_active_after_close"]==0,"CLOCK task was not terminated")
    evidence["pointer_cmd_taskbar"]=move_to(125,754)
    execute("mouse_button 1");time.sleep(0.1);execute("mouse_button 0");time.sleep(0.5)
    before=list(struct.unpack("<2I",read_memory(cmd_base+0xf46c,8)))
    command("cpuid")
    after=list(struct.unpack("<2I",read_memory(cmd_base+0xf46c,8)))
    evidence["cmd_cursor_after_close"]=[before,after];save()
    require(after==[before[0],before[1]+2],"CMD did not return to prompt after CLOCK exit")
    require(read_memory(cmd_base+0xb2da,6)==bytes(6),"CMD input not cleared after CLOCK exit")
    capture("cmd_after_close");save()
    print(f"C CLOCK loaded; {len(parsed.relocations)} relocations, face/labels, three changing frames, PS/2 close, task exit and CMD reuse verified")
