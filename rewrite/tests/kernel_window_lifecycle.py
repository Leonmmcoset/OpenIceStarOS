"""Window create/close parity with real heap/copy and explicit UI boundaries.
Surface allocation, caption rendering, activation, compositor, registry and
delete/hit boundaries compare exact calls/state; actual chains run in QEMU.
"""
import argparse,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(BUILD/'test-deps')]
from tests.kernel_recovery_compare import compare
from tests.kernel_font_outline import fixture,words
from tests.kernel_image_controls import Machine as ImageMachine,state,allocated,HEAP
from tests.kernel_window_services import store,OBJ,SURF,TEXT,OUT,CALLBACK,STYLES
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
SLOTS=[('window_create','kernel_window_create_807a0a',0x807a0a,0x807f72),
 ('window_close','kernel_window_close_81021c',0x81021c,0x81041a)]
BOUNDARIES={0x817626:4,0x8045f0:7,0x8087e0:1,0x8178f0:1,0x81905e:5,0x8105dc:1,0x807f72:1,0x8078f2:2}

class Machine(ImageMachine):
    def boundary(self,uc,address,size,unused):
        if address not in BOUNDARIES:return super().boundary(uc,address,size,unused)
        sp=uc.reg_read(UC_X86_REG_ESP);n=BOUNDARIES[address]
        args=list(struct.unpack('<'+'I'*n,uc.mem_read(sp+4,n*4)));self.events.append((address,args))
        value=0
        if address==0x817626:
            value=self.case.get('surface_result',7)
            self.put(SURF+4,words(*args))
            # Original writes surface ownership before its negative-ID check.
            self.put(0x4ffffc,words(SURF+64))
        elif address==0x8078f2:value=0x87654321
        for a,b in self.case.get('changes',{}).get(address,[]):self.put(a,b)
        uc.reg_write(UC_X86_REG_EAX,value);uc.reg_write(UC_X86_REG_ESP,sp+4)
        uc.reg_write(UC_X86_REG_EIP,struct.unpack('<I',uc.mem_read(sp,4))[0])

def setup():
    return [*state(),store(0x1b090,64),store(0x1b094,0,8),(TEXT,b'Title\0')]

def scenarios():
    cases=[]
    for theme in (0,1,2,3,4,5,6):
        for style in (-1,0,1,3,4):
            for mode in (0,1,2,3,0xffffffff):
                for x,y,w,h in [(10,20,260,180),(0,0,0,0),(-30,-40,7,5),(0x7fffffff,0x80000000,-1,1)]:
                    selected=theme if 0<=style<=3 else 0
                    sx,sy,cw,ch=(6,28,w-10,h-34) if selected in (1,2,3) else (8,32,w-14,h-40) if selected in (4,5) else (0,0,w,h)
                    tx,ty,tw,th=(3,3,w-6,20 if selected<4 else 24) if 1<=selected<=5 else (0,0,0,0)
                    object=HEAP+16
                    memory=[store(object,8,23,0,7),store(object+0x10f,0,style,99,mode,0,1,1,x,y,w,h),
                        store(object+0x13b,sx,sy,cw,ch,tx,ty,tw,th,0),store(0x6a0000+32,object),store(0x1b098,9),store(SURF+48,8)]
                    c=fixture(0,[x,y,w,h,style,99,mode,TEXT],setup()+[(STYLES,words(*([theme]*4)))],8,memory)
                    c['expected_flags']=0x202
                    c['expected_events']=[(0x817626,[x&0xffffffff,y&0xffffffff,*([32,24] if mode==3 else [w&0xffffffff,h&0xffffffff])]),
                        (0x8045f0,[7,object+16,0,style&0xffffffff,99,mode,1])]
                    cases.append(c)
    # Allocator fails before slot validation, but slot is still queried. Both
    # error paths omit caller flag restoration; slot failure frees the object.
    c=fixture(0,[10,20,260,180,1,0,2,TEXT],setup()+[store(0x1b050,HEAP+2048)],-1)
    c.update(expected_flags=2,expected_events=[]);cases.append(c)
    c=fixture(0,[10,20,260,180,1,0,2,TEXT],setup()+[store(0x1b090,8)],-1,[store(HEAP,0,0,0,0)])
    c.update(expected_flags=2,expected_events=[]);cases.append(c)
    for mode in (0,3):
        c=fixture(0,[10,20,260,180,1,0,mode,TEXT],setup(),-1,[store(SURF+64+48,8)])
        c.update(surface_result=0xffffffff,expected_flags=2);cases.append(c)
    for count in (1,3,8):
        c=fixture(0,[1,2,200,100,2,0,1,TEXT],setup()+[store(0x1b094,1,count),store(0x6a0000,7,0,9,0)],1,
            [store(0x1b094,0 if count>1 else 1,count if count>1 else count+1),store(0x6a0000+4,HEAP+16)]);cases.append(c)
    for title in (b'',b'A',b'A'*254,b'A'*255):
        cases.append(fixture(0,[1,2,200,100,2,0,1,TEXT],setup()+[(TEXT,title+b'\0')]))
    # Reused heap payload, copied-title overlap and caption boundary mutation.
    for source in (TEXT,HEAP+16+16,HEAP+16+24):
        cases.append(fixture(0,[1,2,200,100,2,0,1,source],setup()+[(HEAP+16,b'\xa5'*0x7f0),(source,b'Title\0')]))
    c=fixture(0,[1,2,200,100,2,0,1,TEXT],setup())
    c['changes']={0x8045f0:[store(0x1b098,100),store(0x1b094,7),store(HEAP+16+12,1)]};cases.append(c)
    # Full close routing matrix, with independent ordered call expectations.
    for id in (0,7,0x40000007):
        for visible in (0,1,3):
            for active in (0,1,2):
                for hidden in (0,1,2):
                    for registered in (0,1,2):
                        for drag in (0,7):
                            setup0=setup()+[store(0x6a0000,OBJ),store(SURF+44,visible),store(OBJ+0x10f,active),store(OBJ+0x123,hidden),
                                store(OBJ+0x127,registered),store(0x1b138,drag),store(OBJ+0x55f,OUT),store(SURF+64+48,99)]
                            calls=[]
                            if visible:
                                if active==1:calls.append((0x8087e0,[99 if visible==3 else 0]))
                                if hidden==0:calls.append((0x8178f0,[7]))
                                calls.append((0x81905e,[2,10,20,256,192]))
                            if id and registered==1:calls.append((0x8105dc,[id]))
                            calls += [(0x807f72,[id]),(0x8078f2,[10,20])]
                            c=fixture(1,[id],setup0,memory=[store(OUT+8,1),store(0x1b16c,0x87654321),store(0x1b138,0)])
                            c.update(expected_events=calls,expected_flags=0x202);cases.append(c)
    # Actual callback code changes its own copied argument and object fields.
    callback=(b'\x8b\x44\x24\x04\xa3'+words(OUT)+
        b'\xc7\x05'+words(OBJ+0x127)+words(0)+b'\xc7\x44\x24\x04'+words(0xdeadbeef)+b'\xc3')
    c=fixture(1,[7],setup()+[store(OBJ+0x5b7,CALLBACK),(CALLBACK,callback)],memory=[store(OUT,7),store(OBJ+0x127,0)])
    cases.append(c)
    # Close retains the original object pointer across callbacks/table changes
    # and reads its completion pointer after deletion (the original ordering).
    for boundary in (0x8087e0,0x8178f0,0x81905e,0x8105dc,0x807f72,0x8078f2):
        c=fixture(1,[7],setup()+[store(OBJ+0x10f,1),store(OBJ+0x123,0),store(OBJ+0x127,1),store(OBJ+0x55f,OUT)])
        c['changes']={boundary:[store(OBJ+0x55f,OUT+16),store(0x6a0000+28,HEAP+16),store(OBJ+0x123,2)]}
        cases.append(c)
    return cases

def main():
    p=argparse.ArgumentParser(__doc__);p.add_argument('--installed',action='store_true');a=p.parse_args()
    compare('window_lifecycle',SLOTS,Machine,scenarios(),a.installed,[
        'real owner heap and title copy','independent packed object/layout/geometry oracle',
        'all theme styles, screen-sized backing mode, allocation and slot failures',
        'exact surface/caption/activation/compositor/registry/delete/hit boundaries',
        'callback argument-copy isolation and live object/counter changes',
        'post-delete completion-word write and original CLI error paths',
        'all touched memory/heap pages, caller arguments, cdecl registers and x87 control'])
if __name__=='__main__':main()
