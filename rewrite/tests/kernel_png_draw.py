"""Actual PNG/DEFLATE/heap/pixel execution with independent RGBA pixel oracle.
File lookup/read boundaries isolate dispatch; the guest companion uses real FAT.
"""
import argparse,itertools,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(BUILD/'test-deps')]
from tests.kernel_recovery_compare import compare
from tests.kernel_font_outline import fixture,words
from tests.kernel_image_controls import Machine as ImageMachine,state,allocated,HEAP
from tests.kernel_window_services import store,SURF,PIXELS,TEXT,SERVICES
from tests.kernel_png import png
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
DATA=0x6c0000
SLOTS=[('png_surface_draw','kernel_png_surface_draw_824014',0x824014,0x8241d0),
 ('png_direct_draw','kernel_png_direct_draw_8241d0',0x8241d0,0x82436c),
 ('png_file_draw','kernel_png_file_draw_82436c',0x82436c,0x82443e)]
FILES={0x8263c0:2,0x827454:1,0x825c8e:1,0x827032:1,0x826940:1}
class Machine(ImageMachine):
    returns=set()
    def boundary(self,uc,address,size,unused):
        if address not in FILES:
            # No PNG, DEFLATE, quantizer or pixel substitutions.
            if address==0x81f88a:return super().boundary(uc,address,size,unused)
            return
        sp=uc.reg_read(UC_X86_REG_ESP);n=FILES[address]
        args=list(struct.unpack('<'+'I'*n,uc.mem_read(sp+4,n*4)))
        value=0
        if address in (0x8263c0,0x825c8e):
            self.events.append((address,bytes(uc.mem_read(args[0],64)).split(b'\0',1)[0]));value=self.case.get('found',1)
            if address==0x8263c0:
                record=bytearray(32);record[:11]=b'TEST    PNG';struct.pack_into('<I',record,28,self.case['length']&0xffffffff)
                uc.mem_write(args[1],bytes(record))
        elif address in (0x827454,0x827032):
            self.events.append((address,bytes(uc.mem_read(args[0],32)) if address==0x827454 else args[0]))
            value=self.case.get('file_pointer',HEAP+16)
        else:self.events.append((address,args[0]));value=self.case['length']
        uc.reg_write(UC_X86_REG_EAX,value&0xffffffff);uc.reg_write(UC_X86_REG_ESP,sp+4)
        uc.reg_write(UC_X86_REG_EIP,struct.unpack('<I',uc.mem_read(sp,4))[0])
def pattern(w,h):
    colors=((0,0,0,0),(0x11,0x22,0x33,0),(0x44,0x55,0x66,1),(0xff,0x80,0x20,255))
    return bytes(v for i in range(w*h) for v in colors[i%len(colors)])
def encoded(w,h,pixels,level=0,color=6):
    n=w*(4 if color==6 else 3)
    return png(w,h,color,8,b''.join(b'\0'+pixels[y*n:(y+1)*n] for y in range(h)),level=level)
def oracle(depth,w,h,pixels,x,y,service_depth=None):
    out=bytearray(b'\x7e'*(256*192*4))
    storage=depth if service_depth is None else service_depth
    for row in range(h):
        for col in range(w):
            r,g,b,a=pixels[(row*w+col)*4:(row*w+col+1)*4];dx,dy=x+col,y+row
            if not(0<=dx<256 and 0<=dy<192):continue
            color=None
            if depth==32 and a:color=(r<<16)|(g<<8)|b
            if depth==8 and any((r,g,b,a)):
                threshold=((3,1),(0,2))[row&1][col&1]
                color=16+sum(((v*21//256+threshold)//4)*scale for v,scale in ((r,1),(g,6),(b,36)))
            if color is not None:
                if storage==32:out[(dy*256+dx)*4:(dy*256+dx+1)*4]=words(color)
                if storage==8:out[dy*256+dx]=color&255
    return bytes(out)
def scenarios():
    cases=[]
    for fn,depth,geometry,offset,level in itertools.product((0,1),(8,32,16),((1,1),(3,2),(8,5)),
        ((0,0),(3,4),(-2,-1),(255,191),(256,192)),(0,6)):
        w,h=geometry;x,y=offset;pixels=pattern(w,h);data=encoded(w,h,pixels,level)
        c=fixture(fn,[7 if fn==0 else SURF,x,y,DATA,len(data)],state(depth)+[(DATA,data)],
            memory=[(PIXELS,oracle(depth,w,h,pixels,x,y))]);cases.append(c)
    # ID drawing dispatches through the global video-mode service even when
    # target metadata disagrees. Direct drawing bypasses that service table.
    for depth,storage,entry in ((8,32,0x81b2f4),(32,8,0x81b26e)):
        for x,y in ((0,0),(3,4),(-2,-1)):
            pixels=pattern(3,2);data=encoded(3,2,pixels,6)
            cases.append(fixture(0,[7,x,y,DATA,len(data)],state(depth)+[store(SERVICES,entry),(DATA,data)],
                memory=[(PIXELS,oracle(depth,3,2,pixels,x,y,storage))]))
    # Non-RGBA source layouts retain the adapter's original byte assumptions.
    for color,depth in ((0,8),(2,8),(4,8),(6,16)):
        channels={0:1,2:3,4:2,6:4}[color];w,h=3,2
        data=png(w,h,color,depth,bytes((w*channels*depth//8+1)*h))
        for fn in (0,1):cases.append(fixture(fn,[7 if fn==0 else SURF,2,3,DATA,len(data)],state()+[(DATA,data)]))
    # Header errors occur during decode, after the initial error query. The
    # zero dimensions still reach destroy and release the decoder context.
    valid=encoded(3,2,pattern(3,2))
    for fn,length in itertools.product((0,1),(0,1,8,16,32)):
        cases.append(fixture(fn,[7 if fn==0 else SURF,0,0,DATA,length],state()+[(DATA,valid)],memory=[store(HEAP+8,0)]))
    for fn in (0,1):
        for error in (b'bad png',b'\x89PNG\r\n\x1a\n'+bytes(40)):
            cases.append(fixture(fn,[7 if fn==0 else SURF,0,0,DATA,len(error)],state()+[(DATA,error)]))
    # Fully clipped one-pixel decode still runs the actual lifecycle.
    for fn in (0,1):
        data=encoded(1,1,b'\x11\x22\x33\xff')
        cases.append(fixture(fn,[7 if fn==0 else SURF,-1,-1,DATA,len(data)],state()+[(DATA,data)]))
    for path,found,length,pointer in itertools.product((br'C:\TEST.PNG',br'F:\TEST.PNG',br'B:\TEST.PNG',br'G:\TEST.PNG',br'c:\TEST.PNG',b'TEST.PNG',b'\xff:\TEST.PNG'),
        (-1,0,1),(-1,0,len(valid)),(0,HEAP+16)):
        s=state()+allocated()+[(HEAP+16,valid),(TEXT,path+b'\0')]
        c=fixture(2,[7,3,4,TEXT],s);c.update(found=found,length=length,file_pointer=pointer)
        absolute=path[:1] in (b'C',b'D',b'E',b'F') and path[1:3]==b':\\'
        events=[(0x8263c0 if absolute else 0x825c8e,path)]
        if (absolute and found>0) or (not absolute and found>=0):
            if absolute:
                record=bytearray(32);record[:11]=b'TEST    PNG';struct.pack_into('<I',record,28,length&0xffffffff)
                events.append((0x827454,bytes(record)))
            else:events += [(0x827032,found),(0x826940,found)]
            if length>0 and pointer:
                c['expected_memory']=[(PIXELS,oracle(32,3,2,pattern(3,2),3,4))]
        c['expected_events']=events;cases.append(c)
    # Wrapped surface IDs, clipped coordinates and destination metadata alias.
    for fn,target in ((0,0x40000007),(1,SURF)):
        data=encoded(3,2,pattern(3,2))
        for x,y in ((0x7fffffff,0),(0,0x80000000),(-1,-1)):
            cases.append(fixture(fn,[target,x,y,DATA,len(data)],state()+[(DATA,data)]))
        cases.append(fixture(fn,[target,0,0,DATA,len(data)],state()+[(DATA,data),store(SURF,SURF+32)]))
    return cases
def main():
    p=argparse.ArgumentParser(__doc__);p.add_argument('--installed',action='store_true');a=p.parse_args()
    compare('png_draw',SLOTS,Machine,scenarios(),a.installed,[
        'actual PNG/DEFLATE, allocator, dithering and pixel writes without codec/pixel mocks',
        'independent RGBA, alpha-zero versus DWORD transparency, clipping and 8-bit phase oracles',
        'surface-ID/direct destinations, alternate PNG formats, errors and header-error cleanup',
        'global video-mode dispatch versus per-target depth, including mismatched-depth pixel oracles',
        'explicit absolute C-F/relative FAT lookup/read routes, signed lengths and zero reads',
        'all touched heap/data pages, caller arguments, cdecl registers and x87 controls'])
if __name__=='__main__':main()
