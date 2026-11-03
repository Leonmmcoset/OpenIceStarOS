"""Maximize/layout/restore with actual owner allocator and live callbacks.
Only frame drawing and compositor submission are explicit native boundaries.
"""
import argparse,itertools,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(BUILD/'test-deps')]
from tests.kernel_recovery_compare import compare
from tests.kernel_font_outline import fixture,words
from tests.kernel_image_controls import Machine as ImageMachine,state,allocated,HEAP
from tests.kernel_window_services import store,OBJ,SURF,OUT,CALLBACK
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
SLOTS=[('window_maximize','kernel_window_maximize_80ed56',0x80ed56,0x80f4be),
 ('window_layout_all','kernel_window_layout_all_80f4be',0x80f4be,0x80fb12),
 ('window_unmaximize','kernel_window_unmaximize_80fbfc',0x80fbfc,0x81002e)]
BAR=SURF+256
BOUNDARIES={0x8045f0:7,0x81905e:5}
class Machine(ImageMachine):
    returns=set()
    def execute(self,case):
        self.uc.ctl_remove_cache(CALLBACK,CALLBACK+128)
        return super().execute(case)
    def boundary(self,uc,address,size,unused):
        if address not in BOUNDARIES:return super().boundary(uc,address,size,unused)
        sp=uc.reg_read(UC_X86_REG_ESP);n=BOUNDARIES[address]
        args=list(struct.unpack('<'+'I'*n,uc.mem_read(sp+4,n*4)));self.events.append((address,args))
        for a,b in self.case.get('changes',{}).get(address,[]):self.put(a,b)
        uc.reg_write(UC_X86_REG_EAX,0);uc.reg_write(UC_X86_REG_ESP,sp+4)
        uc.reg_write(UC_X86_REG_EIP,struct.unpack('<I',uc.mem_read(sp,4))[0])
def setup(depth=32,capacity=(12,9)):
    return [*state(depth),*allocated(),store(SURF,HEAP+16,10,20,12,9,*capacity),store(SURF+36,depth),
        store(SURF+128+20,32,24),store(0x500000+8,SURF+128),store(0x500000+32,BAR),(BAR,words(0,0,0,4,3)),store(0x1b0a4,8),
        store(OBJ+0x10f,1,1,99,3,0),store(OBJ+0x12b,11,22,7,5,6,28),store(OBJ+0x573,0)]
def layout(bar):
    return {0:(0,0,32,24),1:(0,0,32,21),2:(4,0,28,24),3:(0,0,28,24),4:(0,3,32,21),5:(0,0,32,24),6:(10,20,12,9)}[bar]
def scenarios():
    cases=[]
    for fn,bar,depth,capacity in itertools.product(range(3),range(7),(8,32,-8,16),((12,9),(32,24),(64,48))):
        memory=[];events=[];args=[] if fn==1 else [7]
        s=setup(depth,capacity)+[store(0x1b09c,bar)]
        if fn==2:s += [store(OBJ+0x11b,4,1)]
        if fn==1:s += [store(OBJ+0x11b,4,1)]
        x,y,w,h=layout(bar) if fn<2 else (11,22,7,5)
        memory=[store(SURF+4,x,y,w,h),store(OBJ+0x143,w-12,h-34),store(OBJ+0x153,w-6)]
        if fn==0:memory += [store(OBJ+0x12b,10,20,12,9),store(OBJ+0x11b,4,1)]
        if fn==2:memory += [store(OBJ+0x11b,3,0)]
        if fn<2:memory.append(store(SURF+20,32,24))
        events.append((0x8045f0,[7,OBJ+16,1,1,99,3 if fn==2 else 4,1]))
        if fn!=1:events.append((0x8045f0,[7,OBJ+16,1,1,99,3 if fn==2 else 4,0]))
        if fn==0:events.append((0x81905e,[7,x,y,w,h]))
        elif fn==2 and bar<6:events.append((0x81905e,[2,*layout(bar)]))
        c=fixture(fn,args,s,memory=memory);c['expected_events']=events;cases.append(c)
    # Original gates and exact signed positivity tests.
    for fn,style,mode,status in itertools.product((0,2),(-1,0,1,4),(0,2,3,4),(-1,0,1,2)):
        s=setup()+[store(OBJ+0x113,style),store(OBJ+0x11b,mode,status)]
        c=fixture(fn,[7],s)
        if style<=0 or (mode,status)!=((3,0) if fn==0 else (4,1)):c['expected_events']=[]
        cases.append(c)
    for fn in (0,1):
        for limit in (HEAP+4096,HEAP+8192):
            cases.append(fixture(fn,[] if fn==1 else [7],setup()+[store(0x1b050,limit),store(OBJ+0x11f,1 if fn else 0)]))
        for depth in (-1,-7,-9,0,7,0x7fffffff):
            cases.append(fixture(fn,[] if fn==1 else [7],setup(depth)+[store(OBJ+0x11f,1 if fn else 0)]))
    # Overflowed backing area comparisons stay signed, and size*depth wraps.
    for width,height in ((0,0),(0x10000,0x10000),(0x80000000,1),(0xffffffff,1)):
        for fn in (0,1):cases.append(fixture(fn,[] if fn==1 else [7],setup()+[store(SURF+20,width,height),store(OBJ+0x11f,1 if fn else 0)]))
    # Actual callback observes changed mode and writes its private argument.
    callback=(b'\x8b\x44\x24\x04\xa3'+words(OUT)+b'\xc7\x44\x24\x04'+words(0xdeadbeef)+
        b'\xc7\x05'+words(OBJ+0x117)+words(77)+b'\xc3')
    for fn in range(3):
        s=setup()+[store(OBJ+0x573,CALLBACK),(CALLBACK,callback)]
        if fn:s += [store(OBJ+0x11b,4,1)]
        cases.append(fixture(fn,[] if fn==1 else [7],s,memory=[store(OUT,7),store(OBJ+0x117,77)]))
    for fn,boundary in itertools.product(range(3),BOUNDARIES):
        s=setup()+[store(OBJ+0x11f,1 if fn else 0),store(OBJ+0x11b,4 if fn else 3)]
        c=fixture(fn,[] if fn==1 else [7],s)
        c['changes']={boundary:[store(OBJ+12,8),store(OBJ+0x117,77),store(0x1b09c,4)]};cases.append(c)
    # Layout loop rereads count. It skips null and negative table pointers,
    # but still invokes a state-zero callback even for an undecorated object.
    for count in (-1,0,1,8,9):
        for status in (-1,0,1,2):
            s=setup()+[store(0x1b098,count),store(0x6a0000,0xffffffff,0),store(OBJ+0x11f,status)]
            cases.append(fixture(1,[],s))
    for style in (-1,0,1):
        s=setup()+[store(OBJ+0x113,style),store(OBJ+0x573,CALLBACK),(CALLBACK,callback)]
        cases.append(fixture(1,[],s,memory=[store(OUT,7)]))
    # A state-one callback changing state to zero is called a second time.
    change=(b'\xff\x05'+words(OUT)+b'\xc7\x05'+words(OBJ+0x11f)+words(0)+b'\xc3')
    s=setup()+[store(OBJ+0x11f,1),store(OBJ+0x573,CALLBACK),(CALLBACK,change)]
    cases.append(fixture(1,[],s,memory=[store(OUT,2)]))
    # A live callback may terminate the table walk by reducing its count.
    change=b'\xc7\x05'+words(0x1b098)+words(0)+b'\xc3'
    s=setup()+[store(OBJ+0x573,CALLBACK),(CALLBACK,change)]
    cases.append(fixture(1,[],s,memory=[store(0x1b098,0)]))
    for fn in (0,2):cases.append(fixture(fn,[0x40000007],setup()+[store(OBJ+0x11f,1 if fn==2 else 0),store(OBJ+0x11b,4 if fn==2 else 3)]))
    return cases
def main():
    p=argparse.ArgumentParser(__doc__);p.add_argument('--installed',action='store_true');a=p.parse_args()
    compare('window_geometry',SLOTS,Machine,scenarios(),a.installed,[
        'actual owner-tagged allocator/free, growth/failure and signed wrapped sizing',
        'independent six-way taskbar layout, saved geometry and client/title expectations',
        'live frame/compositor call order, real callbacks and private callback arguments',
        'live table count, signed slot pointers, state changes and duplicate callback quirk',
        'all touched heap/data pages, caller arguments, cdecl registers and x87 controls'])
if __name__=='__main__':main()
