"""Original/ordinary/installed window focus and exact title-button rectangles.
Surface ordering, caption and refresh calls are explicit native boundaries;
the guest companion executes the real compositor and live callback chain.
"""
import argparse,itertools,random,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build'
sys.path[:0]=[str(ROOT),str(BUILD/'test-deps')]
from tests.kernel_recovery_compare import compare
from tests.kernel_font_outline import fixture,words
from tests.kernel_window_services import Machine as WindowMachine,base,store,OBJ,SURF,OUT,CALLBACK,STYLES
SLOTS=[('window_raise','kernel_window_raise_8084a0',0x8084a0,0x8087e0),
 ('window_activate','kernel_window_activate_8087e0',0x8087e0,0x808b78),
 ('window_deactivate','kernel_window_deactivate_808b78',0x808b78,0x808db8),
 ('window_button_hit','kernel_window_button_hit_808db8',0x808db8,0x80971e)]
PARENT,CURRENT,BAR=OBJ+0x1000,OBJ+0x2000,SURF+128
BOUNDARIES={0x8179d6:1,0x817b32:2,0x817a3c:1,0x807540:2,0x81905e:5}
class Machine(WindowMachine):
    returns={3}
def state():
    return [*base(),store(0x6a0000,PARENT),store(0x6a0000+20,PARENT),store(0x6a0000+36,CURRENT),
        (PARENT,bytes(0x600)),(CURRENT,bytes(0x600)),store(PARENT,5),store(PARENT+12,8),
        store(PARENT+0x113,1),store(PARENT+0x157,20),store(CURRENT,9),store(CURRENT+8,0),
        store(0x500000+32,BAR),(BAR,words(0,1,2,300,28,300,28,0,0,32,255,4)),
        store(0x1b0a4,8),store(OBJ+0x55f,OUT)]
def case(fn,args,setup=(),memory=(),expected=None,calls=None):
    c=fixture(fn,args,[*state(),*setup],expected,memory)
    c['boundaries']={a:(n,0,[]) for a,n in BOUNDARIES.items()}
    if calls is not None:c['expected_calls']=calls
    return c
def signed(v):return (v&0x7fffffff)-(v&0x80000000)

def scenarios():
    cases=[]
    # Stack routing matrix: unsigned z order, pinned/drag windows, hidden
    # surfaces, repeated activation, and conditional notifications.
    for fn in (0,1):
        for z,bar,cursor,special,active in itertools.product((0,1,4,8,0xffffffff),(0,4,0xffffffff),(0,3),(0,1,2),(0,1,2)):
            setup=[store(SURF+44,z),store(BAR+44,bar),store(SURF+64+44,cursor),
                store(OBJ+0x10f,active,2),store(OBJ+0x11f,4 if special==1 else 0,1,1),store(0x1b150,7 if special==2 else 0)]
            calls=[];memory=[]
            if fn==0 or active==0:
                if fn==1:memory += [store(OBJ+0x10f,1),store(0x1b0a0,7),store(OUT,1)]
                memory += [store(0x1b0ec,1)]
                if not z:calls.append((0x8179d6,[7]))
                if bar:
                    if special:calls.append((0x817b32,[7,(bar+1)&0xffffffff]))
                    elif z!=bar:calls.append((0x817b32,[7,bar if z>bar else (bar-1)&0xffffffff]))
                    elif cursor:calls.append((0x817a3c,[1]))
                elif cursor and z!=cursor:calls.append((0x817b32,[7,cursor if z>cursor else cursor-1]))
                if special==1 and (bar or cursor):memory.append(store(0x1b138,7))
                if fn==1:calls.append((0x807540,[7,1]))
            calls.append((0x81905e,[7,10,20,256,192]))
            if fn==0:memory.append(store(OBJ+0x123,0))
            cases.append(case(fn,[7],setup,memory,calls=calls))
    for fn in (0,1):
        cases.append(case(fn,[8],calls=[]))
        for sid,registered,style in itertools.product((2,3,7),(0,1,2),(-1,0,1,3,4)):
            setup=[store(OBJ+12,sid),store(0x500000+sid*4,SURF),store(OBJ+0x127,registered),store(OBJ+0x113,style),store(SURF+44,0)]
            cases.append(case(fn,[7],setup))
        for hidden in (0,1,2):
            for pointer in (0,OUT,OBJ+0x10f,OBJ+0x113):
                cases.append(case(fn,[0x40000007],[store(OBJ+0x123,hidden),store(OBJ+0x55f,pointer)]))
        for boundary in BOUNDARIES:
            c=case(fn,[7],[store(SURF+44,0)])
            c['boundaries'][boundary]=(BOUNDARIES[boundary],0,[store(OBJ+12,8),store(OBJ+0x123,1),store(OBJ+0x55f,OUT+16)])
            cases.append(c)
    # Deactivation reads parent/current objects before testing active state.
    for active,parent,child,style,pstyle,notify in itertools.product((0,1,2),(0,5),(0,7),(-1,0,1,3,4),(0,1,4),(0,OUT)):
        setup=[store(OBJ+8,parent),store(OBJ+0x10f,active,style),store(PARENT+0x113,pstyle),store(CURRENT+8,child),store(OBJ+0x55f,notify)]
        calls=[];memory=[]
        if active:
            memory=[store(OBJ+0x10f,0)]+([store(OUT+4,1)] if notify else [])
            if child!=7:
                if style in (1,2,3):calls.append((0x807540,[7,0]))
                if parent>0 and pstyle in (1,2,3):calls.extend([(0x807540,[parent,0]),(0x81905e,[8,1,2,300,26])])
            calls.append((0x81905e,[7,10,20,256,192]))
        cases.append(case(2,[7],setup,memory,calls=calls))
    cases.append(case(2,[8],calls=[]))
    # Actual x86 callback writes its own outgoing argument and live fields.
    callback=b'\x8b\x44\x24\x04\xa3'+words(OUT)+b'\xc7\x44\x24\x04'+words(0xdeadbeef)+b'\xc7\x05'+words(OBJ+0x55f)+words(OUT+16)+b'\xc3'
    cases.append(case(2,[7],[store(OBJ+0x10f,1),store(OBJ+0x5a7,CALLBACK),(CALLBACK,callback)],
        [store(OUT,7),store(OUT+20,1)]))
    for boundary in (0x807540,0x81905e):
        c=case(2,[7],[store(OBJ+0x10f,1),store(OBJ+8,5)])
        c['boundaries'][boundary]=(BOUNDARIES[boundary],0,[store(OBJ+12,8),store(OBJ+0x55f,OUT+16),store(0x6a0000+28,CURRENT)])
        cases.append(c)
    # Independent half-open integer-cell oracle for each button region.
    # Explicit lines on every edge, negative and wrapped screen coordinates.
    for theme,mode,geometry in itertools.product((-1,0,1,2,3,4,5,6),(-1,0,1,2,3,0x7fffffff),
        ((10,20,256),(-30,-40,72),(0x7ffffff0,0x7ffffff8,64))):
        sx,sy,width=geometry;wide=theme in (4,5);shift=3 if wide else 0
        # Cells carry the returned original API code, not an inferred action.
        regions=[]
        if 1<=theme<=5 and mode>=1:
            regions=[(1,24+shift,8+shift)]
            if mode==2:regions.append((4,40+shift,24+shift))
            elif mode>2:regions.extend([(2,59+shift,40+shift),(3,40+shift,24+shift)])
        xs=set(signed(sx+width-o) for o in (7,8,9,10,11,12,23,24,25,26,27,28,39,40,41,42,43,44,58,59,60,61,62,63))
        ys=[signed(sy+o) for o in (4,5,6,7,8,14,15,16,18,19,20)]
        for x,y in itertools.product(sorted(xs),ys):
            expected=next((code for code,l,r in regions if
                signed(sx+width-l)<x<signed(sx+width-r) and signed(sy+(7 if wide else 5))<y<signed(sy+(19 if wide else 15))),0)
            cases.append(case(3,[7,x,y],[store(SURF+4,sx,sy,width),store(OBJ+0x11b,mode),(STYLES,words(*([theme]*4)))],expected=expected,calls=[]))
    rng=random.Random(0x808db8)
    for _ in range(128):
        x,y,width=[rng.getrandbits(32) for _ in range(3)]
        cases.append(case(3,[7,signed(x),signed(y)],[store(SURF+4,rng.getrandbits(32),rng.getrandbits(32),width),store(OBJ+0x11b,3)]))
    return cases

def main():
    p=argparse.ArgumentParser(__doc__);p.add_argument('--installed',action='store_true');a=p.parse_args()
    compare('window_focus',SLOTS,Machine,scenarios(),a.installed,[
        'independent ordered stack/caption/refresh and notification oracles',
        'unsigned z ordering, hidden/pinned/drag/cursor surfaces and reserved IDs',
        'parent/current object state, repeated activation and live callback argument isolation',
        'all theme classes and modes; strict button edges and signed wrapping',
        'all touched memory, caller arguments, cdecl registers and x87 controls'])
if __name__=='__main__':main()
