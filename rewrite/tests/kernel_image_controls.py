"""Real heap/strcpy/pixel chains, with explicit flag/FAT/decoder boundaries.
The decoder boundaries test routing and ownership only; the companion QEMU
test executes actual FAT, allocation, codecs and pixels without substitutions.
"""
import argparse,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(BUILD/'test-deps')]
from tests.kernel_recovery_compare import compare
from tests.kernel_font_outline import fixture,words
from tests.kernel_font_heap import Machine as HeapMachine
from tests.kernel_window_services import base,store,OBJ,SURF,PIXELS,IMAGE,IMAGES,TEXT,THEME
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
HEAP=0x2000000
SLOTS=[('image_create','kernel_image_create_8121d4',0x8121d4,0x8125dc),
 ('image_release','kernel_image_release_8125dc',0x8125dc,0x812748),
 ('image_file','kernel_image_file_81282e',0x81282e,0x8129ca),
 ('image_draw','kernel_image_draw_8129ca',0x8129ca,0x812bfa)]
CODECS={0x8239a0:3,0x820d66:2,0x824c70:3,0x82af4c:2}

class Machine(HeapMachine):
    returns={0};mutable_caller=set();aggregates=set()
    def __init__(self,*args):
        super().__init__(*args);self.uc.hook_add(UC_HOOK_CODE,self.boundary)
    def boundary(self,uc,address,size,unused):
        if address not in (0x81a24a,0x81a242,0x81a24d,0x8263c0,0x827454,0x81f88a) and address not in CODECS:return
        sp=uc.reg_read(UC_X86_REG_ESP)
        def args(n):return list(struct.unpack('<'+'I'*n,uc.mem_read(sp+4,n*4)))
        value=0
        if address==0x81a24a:value=self.flags
        elif address==0x81a242:self.flags&=~0x200
        elif address==0x81a24d:self.flags=args(1)[0]
        elif address==0x8263c0:
            path,record=args(2);raw=bytes(uc.mem_read(path,64)).split(b'\0',1)[0]
            self.events.append(('lookup',raw));value=self.case.get('found',0)
            if value:
                content=bytearray(32);content[:8]=b'IMAGE   ';content[8:11]=self.case.get('extension',b'PNG')
                struct.pack_into('<I',content,28,1234);uc.mem_write(record,bytes(content))
        elif address==0x827454:
            record=args(1)[0];self.events.append(('read',bytes(uc.mem_read(record,32))))
            value=self.case.get('file_pointer',HEAP+3*1024+16)
        elif address==0x81f88a:self.events.append(('text',args(6)))
        else:
            values=args(CODECS[address]);self.events.append((address,values))
            for a,b in self.case.get('decoder_changes',[]):self.put(a,b)
        uc.reg_write(UC_X86_REG_EAX,value);uc.reg_write(UC_X86_REG_ESP,sp+4)
        uc.reg_write(UC_X86_REG_EIP,struct.unpack('<I',uc.mem_read(sp,4))[0])
    def execute(self,case):
        self.case=case;self.events=[];self.flags=case.get('flags',0x202)
        result=super().execute(case)
        if 'expected_flags' in case:assert self.flags==case['expected_flags'],(self.label,self.flags,case)
        if 'expected_events' in case:assert self.events==case['expected_events'],(self.label,self.events,case['expected_events'])
        return result,self.events,self.flags

def state(depth=32):
    return [*base(depth&0xffffffff),store(0x500000,SURF+128),store(SURF+128+12,32,24),store(SURF+128+36,depth),
        store(OBJ+4,23),store(OBJ+0x15b,0),store(0x1b128,64),store(0x1b12c,0,0),
        store(HEAP,0,0,0,0),store(0x1b050,HEAP+0x100000),store(0x1b0d8,23)]

def allocated():
    setup=[]
    for i in range(4):setup.append(store(HEAP+i*1024,HEAP+(i+1)*1024,HEAP+(i-1)*1024 if i else 0,1,23))
    return setup+[store(HEAP+4096,0,HEAP+3072,0,0)]

def scenarios():
    cases=[]
    # Actual owner-tagged heap allocations, inclusive slot scan and copied path.
    for depth in (8,32,-8,7):
        for mode in (-1,0,1,2,3):
            for w,h in ((0,0),(1,1),(7,5),(-1,1),(0x10000,0x10000)):
                for path in (0,TEXT):
                    setup=state(depth)+[(IMAGES,bytes(64*4))]
                    c=fixture(0,[7,3,4,w,h,mode,path],setup)
                    cases.append(c)
    # Deterministic error/partial-state oracles: flags, heap and child list.
    c=fixture(0,[7,3,4,8,6,0,0],state()+[store(0x1b050,HEAP+1024)],-1,
        [store(OBJ+0x15b,0),store(IMAGES,0)]);c['expected_flags']=2;cases.append(c)
    c=fixture(0,[7,3,4,8,6,0,0],state()+[store(0x1b128,0)],-1,
        [store(OBJ+0x15b,0),store(HEAP,0,0,0,0)]);c['expected_flags']=2;cases.append(c)
    for count in (254,255,256,0x80000000):
        # Negative child counts are tested with offset wrapping back into a
        # mapped page; huge positive child lists are not traversed.
        c=fixture(0,[7,3,4,8,6,0,0],state()+[store(OBJ+0x15b,count)])
        cases.append(c)
    c=fixture(0,[7,3,4,8,6,0,0],state()+[store(0x1b050,HEAP+2048)],-1,
        [store(OBJ+0x15b,1,0),store(IMAGES,0)]);c['expected_flags']=2;cases.append(c)
    for free,count in ((1,4),(1,1),(0,4)):
        c=fixture(0,[7,3,4,8,6,0,0],state()+[store(IMAGES,99,0,98,97),store(0x1b12c,free,count)])
        cases.append(c)
    for depth in (8,32):
        for mode in (0,1,2):
            w,h=7,5;image=HEAP+16;pixels=HEAP+1040
            width,height=(w,h) if mode<2 else (32,24)
            memory=[store(image,0,7,0,mode,1),store(image+28,pixels,3,4,w,h,width,height),
                store(image+64,depth),store(IMAGES,image),store(OBJ+0x15b,1,0),store(0x1b130,1),
                store(HEAP+12,23),store(HEAP+1024+12,23)]
            c=fixture(0,[7,3,4,w,h,mode,0],state(depth),0,memory);c['expected_flags']=0x202;cases.append(c)
    # The owner allocator does not clear reused payload. Negative modes may
    # publish a preexisting pixel pointer; stale unused fields stay untouched.
    for mode in (-1,0x80000000):
        setup=state()+[store(HEAP+16+28,PIXELS),store(HEAP+16+0x153,0),store(HEAP+16+48,91,92)]
        c=fixture(0,[7,3,4,7,5,mode,0],setup,0,[store(HEAP+16+28,PIXELS),store(HEAP+16+48,91,92)])
        c['expected_flags']=0x202;cases.append(c)
    # File lookup/read/decoder routing boundaries retain actual allocations.
    for found in (0,1,-1):
        for extension in (b'JPG',b'BMP',b'PNG',b'ICO',b'png',b'TXT'):
            for pointer in (0,HEAP+3*1024+16):
                for fn in (0,2):
                    setup=state()+allocated()+[store(IMAGES+28,HEAP+16),
                        store(HEAP+16+0x153,HEAP+2*1024+16),store(HEAP+16+28,HEAP+1024+16)] if fn==2 else state()
                    c=fixture(fn,[7,TEXT] if fn==2 else [7,3,4,8,6,0,TEXT],setup)
                    record=bytearray(32);record[:8]=b'IMAGE   ';record[8:11]=extension;struct.pack_into('<I',record,28,1234)
                    events=[('lookup',br'C:\ICESTAR\COLOR.X')]
                    if found>0:
                        events.append(('read',bytes(record)))
                        codec={b'JPG':0x8239a0,b'BMP':0x820d66,b'PNG':0x824c70,b'ICO':0x82af4c}.get(extension)
                        if pointer and codec:
                            values=[HEAP+16+28,pointer]+([1234] if CODECS[codec]==3 else [])
                            events.append((codec,values))
                    c.update(found=found,extension=extension,file_pointer=pointer,expected_events=events,
                        expected_flags=2 if fn==0 and found>0 and not pointer else 0x202);cases.append(c)
    c=fixture(0,[7,3,4,8,6,0,TEXT],state(),-1,[store(OBJ+0x15b,1,0),store(IMAGES,0)])
    c.update(found=1,file_pointer=0,expected_flags=2);cases.append(c)
    # Old-file pointer stays stale on lookup failure; path changes nonetheless.
    c=fixture(2,[7,TEXT],state()+allocated()+[store(IMAGES+28,HEAP+16),store(HEAP+16+0x153,HEAP+2064)],
        memory=[store(HEAP+16+0x153,HEAP+2064),(HEAP+16+84,br'C:\ICESTAR\COLOR.X'+b'\0')]);cases.append(c)
    cases.append(fixture(2,[8,TEXT],state()))
    for path in (HEAP+16+84,HEAP+16+0x153):
        setup=state()+allocated()+[store(IMAGES+28,HEAP+16),store(HEAP+16+0x153,HEAP+2064),
            (HEAP+16+84,b'old.png\0')]
        cases.append(fixture(2,[7,path],setup))
    # Release preserves original off-by-one child copy and never increments
    # the image free-slot counter, even when the child was not found.
    for count in (-1,0,1,2,5):
        for index in range(max(1,count+1)):
            for file in (0,HEAP+2064):
                for pixels in (0,HEAP+1040):
                    children=[100+i for i in range(max(1,count+1))]
                    if index<count:children[index]=7
                    setup=state()+allocated()+[store(IMAGES+28,HEAP+16),store(HEAP+20,7),
                        store(HEAP+16+28,pixels),store(HEAP+16+0x153,file),store(OBJ+0x15b,count,*children)]
                    c=fixture(1,[7],setup);c['expected_flags']=0x202;cases.append(c)
    c=fixture(1,[8],state());c['expected_flags']=2;cases.append(c)
    c=fixture(3,[7],state()+[store(IMAGES+28,0xffffffac),store(0xffffffb0,7)])
    c['expected_events']=[('text',[7,88,96,0,8,0x83cd67])];cases.append(c)
    for fn,args in ((0,[0x40000007,3,4,7,5,0,0]),(2,[0x40000007,TEXT]),(3,[0x40000007])):
        cases.append(fixture(fn,args,state()+[store(IMAGE+4,7),(IMAGE+84,b'X\0')]))
    # Actual pixel services, independent target-buffer oracle, signed clipping,
    # depth mismatch and placeholder dispatch. No pixel boundary substitutions.
    for depth in (8,32):
        for source_depth in (8,32,16):
            for x,y in ((0,0),(3,4),(-2,-1),(255,191),(256,192)):
                for w,h in ((0,0),(1,1),(7,5),(-1,2),(2,-1)):
                    for first in (b'X',b' ',b'\0'):
                        source=bytes((i*17+13)&255 for i in range(256))
                        setup=state(depth)+[store(IMAGE+4,7),store(IMAGE+28,HEAP+16,x,y,w,h),store(IMAGE+64,source_depth),
                            (HEAP+16,source),(IMAGE+84,first+b'\0')]
                        out=bytearray(b'\x7e'*(256*192*4))
                        if first==b'X' and source_depth==depth:
                            n=depth//8
                            for py in range(max(0,h)):
                                for px in range(max(0,w)):
                                    dx,dy=x+px,y+py
                                    if 0<=dx<256 and 0<=dy<192:
                                        off=(dy*256+dx)*n;src=(py*w+px)*n;out[off:off+n]=source[src:src+n]
                        c=fixture(3,[7],setup,memory=[(PIXELS,bytes(out))]);cases.append(c)
                        if first!=b'X':c['expected_events']=[('text',[7,88,96,0,8,0x83cd67])]
    # Forward source/target overlap, source metadata writes and caller aliasing.
    for depth in (8,32):
        for delta in (-4,0,4):
            setup=state(depth)+[store(IMAGE+4,7),store(IMAGE+28,PIXELS+256+delta,1,1,7,3),
                store(IMAGE+64,depth),(IMAGE+84,b'X\0')]
            cases.append(fixture(3,[7],setup))
        for field in (32,36,40,44):
            setup=state(depth)+[store(IMAGE+4,7),store(IMAGE+28,HEAP+16,0,0,1,1),store(IMAGE+64,depth),
                (IMAGE+84,b'X\0'),store(SURF,IMAGE+field),(HEAP+16,words(0))]
            cases.append(fixture(3,[7],setup))
    return cases

def main():
    p=argparse.ArgumentParser(__doc__);p.add_argument('--installed',action='store_true');a=p.parse_args()
    compare('image_controls',SLOTS,Machine,scenarios(),a.installed,[
        'real owner-tagged heap allocation/free and memcpy/strcpy',
        'explicit flags/FAT lookup/read/decoder routing boundaries',
        'failure leaks, child-list compaction and stale file metadata',
        'real 8/32bpp pixels with independent final-buffer oracle',
        'signed geometry, wrapped multiplication, depth mismatch, overlap and live metadata',
        'all touched data/heap pages, caller arguments, cdecl registers and x87 control'])
if __name__=='__main__':main()
