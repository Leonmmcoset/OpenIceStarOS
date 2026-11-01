"""Window state/routing parity and real pixels. Explicit external boundaries
model task scheduling, icon I/O, legacy caption/close dispatch and compositor
calls; their exact argument order/state is compared, not counted as recovery.
Pure hit tests, slot search, mailbox copy, frames/borders and image fill execute
real original/ordinary/installed chains without interception.
"""
import argparse,json,struct,sys,random
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build';sys.path[:0]=[str(ROOT),str(BUILD/'test-deps')]
from tests.kernel_recovery_compare import compare
from tests.kernel_font_outline import fixture,words
from tests.kernel_font_heap import Machine as HeapMachine
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_READ
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
SLOTS=[['ui_mailbox_put', 'kernel_ui_mailbox_put_802e64', 8400484, 8400582], ['ui_mailbox_get', 'kernel_ui_mailbox_get_802ec6', 8400582, 8400626], ['ui_launch_path', 'kernel_ui_launch_path_802ef2', 8400626, 8400706], ['ui_launch_relative', 'kernel_ui_launch_relative_802f42', 8400706, 8400800], ['ui_frame', 'kernel_ui_frame_807302', 8418050, 8418624], ['ui_caption', 'kernel_ui_caption_807540', 8418624, 8418862], ['ui_title_hit', 'kernel_ui_title_hit_80762e', 8418862, 8419024], ['ui_client_hit', 'kernel_ui_client_hit_8076d0', 8419024, 8419402], ['ui_surface_hit', 'kernel_ui_surface_hit_80784a', 8419402, 8419570], ['ui_find_hit', 'kernel_ui_find_hit_8078f2', 8419570, 8419728], ['ui_object_slot', 'kernel_ui_object_slot_807990', 8419728, 8419850], ['ui_delete', 'kernel_ui_delete_807f72', 8421234, 8421374], ['ui_icon_file', 'kernel_ui_icon_file_807ffe', 8421374, 8421520], ['ui_icon_pointer', 'kernel_ui_icon_pointer_808090', 8421520, 8421610], ['ui_restore', 'kernel_ui_restore_8080ea', 8421610, 8421866], ['ui_hide', 'kernel_ui_hide_8081ea', 8421866, 8422086], ['ui_show', 'kernel_ui_show_8082c6', 8422086, 8422264], ['ui_remove', 'kernel_ui_remove_808378', 8422264, 8422560], ['ui_event4', 'kernel_ui_event4_809e24', 8429092, 8429176], ['ui_event3', 'kernel_ui_event3_80ac4e', 8432718, 8432796], ['ui_move', 'kernel_ui_move_80ac9c', 8432796, 8433232], ['ui_event3b', 'kernel_ui_event3b_80ecba', 8449210, 8449288], ['ui_event3c', 'kernel_ui_event3c_80ed08', 8449288, 8449366], ['ui_redraw_all', 'kernel_ui_redraw_all_80fb12', 8452882, 8453116], ['ui_minimize', 'kernel_ui_minimize_81002e', 8454190, 8454504], ['ui_close_api', 'kernel_ui_close_api_810168', 8454504, 8454524], ['ui_dialog_click', 'kernel_ui_dialog_click_811c46', 8461382, 8461894], ['ui_double_border', 'kernel_ui_double_border_811e4c', 8461900, 8462022], ['ui_image_slot', 'kernel_ui_image_slot_81215a', 8462682, 8462804], ['ui_image_fill', 'kernel_ui_image_fill_812748', 8464200, 8464430], ['ui_image_release_api', 'kernel_ui_image_release_api_812bfa', 8465402, 8465422]]
OBJ,SURF,PIXELS,THEME,STYLES,SERVICES,IMAGE,IMAGES,MAIL,TEXT,OUT,CALLBACK=0x600000,0x620000,0x640000,0x680000,0x681000,0x682000,0x683000,0x684000,0x685000,0x686000,0x687000,0x220000
BOUNDARIES={0x814688:7,0x806742:4,0x806228:4,0x805e84:4,0x81783e:1,0x815978:1,
 0x827798:1,0x826f38:1,0x8179d6:1,0x817b32:2,0x81905e:5,0x8178f0:1,
 0x8087e0:1,0x808b78:1,0x8191d8:7,0x8045f0:7,0x81021c:1,0x8125dc:1}
class Machine(HeapMachine):
    returns={6,7,8,9,10,28};aggregates=set();mutable_caller=set()
    def __init__(self,*args):
        super().__init__(*args);self.uc.hook_add(UC_HOOK_CODE,self.boundary);self.uc.hook_add(UC_HOOK_MEM_READ,self.mailread,begin=MAIL,end=MAIL+255)
    def mailread(self,uc,access,address,size,value,unused):
        if self.case.get('busy_mailbox'):
            self.reads+=1
            if self.reads==20:self.put(MAIL,bytes(256))
    def boundary(self,uc,address,size,unused):
        if address not in self.case.get('boundaries',{}):return
        n,result,changes=self.case['boundaries'][address];sp=uc.reg_read(UC_X86_REG_ESP)
        args=list(struct.unpack('<'+'I'*n,uc.mem_read(sp+4,n*4)));self.calls.append((address,args))
        for a,b in changes:self.put(a,b)
        uc.reg_write(UC_X86_REG_EAX,result);uc.reg_write(UC_X86_REG_ESP,sp+4)
        uc.reg_write(UC_X86_REG_EIP,struct.unpack('<I',uc.mem_read(sp,4))[0])
    def execute(self,case):
        self.case=case;self.calls=[];self.reads=0;self.uc.ctl_remove_cache(CALLBACK,CALLBACK+128);result=super().execute(case)
        if 'expected_calls' in case:assert self.calls==case['expected_calls'],(self.label,self.calls,case['expected_calls'])
        return result,self.calls

def store(address,*values):return (address,words(*values))
INITIAL_PIXELS=b'\x7e'*(256*192*4)
def base(depth=32):
    surf=bytearray(64);struct.pack_into('<7I',surf,0,PIXELS,10,20,256,192,256,192);struct.pack_into('<4I',surf,36,depth,0xff,3,7)
    obj=bytearray(0x600);struct.pack_into('<I',obj,12,7);obj[16:21]=b'Name\0'
    for off,value in [(0x113,2),(0x11b,0x12345678),(0x14b,3),(0x14f,24),(0x153,180),(0x157,140)]:struct.pack_into('<I',obj,off,value)
    services=bytearray(80)
    for off,value in [(0,0x81b2f4 if depth==32 else 0x81b26e),(8,0x81bd26 if depth==32 else 0x81ba8a),(12,0x81c1e4 if depth==32 else 0x81c058),(68,0x817322)]:struct.pack_into('<I',services,off,value)
    return [store(0x6a0000+28,OBJ),(OBJ,bytes(obj)),store(0x500000+28,SURF),store(0x500000+4,SURF+64),(SURF,bytes(surf)),(SURF+64,bytes(surf)),
        (PIXELS,INITIAL_PIXELS),store(0x1b0e0,THEME),(THEME,words(*range(256))),store(0x1b178,STYLES),(STYLES,words(*range(32))),
        store(0x1b0e4,SERVICES),(SERVICES,bytes(services)),store(0x1b0a0,9),store(0x1b0f0,16,0,OUT),store(0x1b098,8),
        store(0x1b134,MAIL),(MAIL,bytes(256)),(TEXT,br'C:\ICESTAR\COLOR.X'+b'\0'),store(0x1b124,IMAGES),store(IMAGES+28,IMAGE),(IMAGE,bytes(68)),
        store(IMAGE+28,PIXELS),store(IMAGE+48,12,9),store(IMAGE+64,depth),store(0x510000,3,1,2,7),store(0x500000+8,SURF+64)]
def bounded(fn,args,setup=(),result=0,changes=(),expected=None,memory=()):
    c=fixture(fn,args,[*base(),*setup],memory=memory)
    c['boundaries']={a:(n,result,list(changes)) for a,n in BOUNDARIES.items()}
    if expected is not None:c['expected_calls']=expected
    return c

def scenarios():
    cases=[];rng=random.Random(0x80762e)
    # Exhaust exact strict hit boundaries and signed wrapped coordinates.
    for sx,sy,w,h in [(10,20,256,192),(0,0,24,24),(-30,-20,64,48),(0x7ffffff0,0x7ffffff0,64,48)]:
        xs=[sx+i for i in (-1,0,1,3,4,21,w-23,w-22,w-21,w-1,w,w+1)]
        ys=[sy+i for i in (-1,0,1,3,4,19,20,21,h-1,h,h+1)]
        signed=lambda x:struct.unpack('<i',words(x))[0]
        for fn in (6,8):
            for x in xs:
                for y in ys:
                    x,y=signed(x),signed(y)
                    valid=(signed(sx+(3 if fn==6 else 0))<x<signed(sx+w-(22 if fn==6 else 0)) and signed(sy+(3 if fn==6 else 0))<y<signed(sy+(20 if fn==6 else h)))
                    cases.append(fixture(fn,[7,x,y],[*base(),store(SURF+4,sx,sy,w,h)],int(valid)))
    for key in (0,0x7e,0x17e,0xffffffff):
        for x in (10,13,14,100,192,193,194):
            for y in (20,44,45,100,183,184,185):
                valid=key!=0x7e and 13<x<193 and 44<y<184
                cases.append(fixture(7,[7,x,y],[*base(),store(SURF+40,key)],int(valid)))
    for cursor in (0,1,3):
        for x,y in ((11,21),(10,20),(265,211),(266,212),(100,100),(0,0)):
            setup=[*base(),store(SURF+64+44,cursor),store(SURF+48,77),store(SURF+64+48,66)]
            cases.append(fixture(9,[x,y],setup,77 if cursor==0 and 10<x<266 and 20<y<212 else 66 if 10<x<266 and 20<y<212 else -1))
    for fn in (10,28):
        for free in (-1,0,1,3):
            for count in (-1,0,1,3,8):
                for capacity in (-1,0,3,8):
                    table=0x6a0000 if fn==10 else IMAGES
                    setup=base()+[store(table,1,2,0,4,0,5,6,7,0)]
                    setup += [store(0x1b094,free,count),store(0x1b090,capacity)] if fn==10 else [store(0x1b12c,free,count),store(0x1b128,capacity)]
                    expected=2 if free>0 and count>=2 else count if count<capacity else -1
                    cases.append(fixture(fn,[],setup,expected))
    # Mailbox uses real forward strcpy. Includes flag/destination alias.
    for raw in (b'',b'A',b'cmd.x',br'C:\ICESTAR\CLOCK.X',bytes(range(1,255))):
        cases.append(fixture(0,[TEXT],[*base(),(TEXT,raw+b'\0')],memory=[(MAIL,raw+b'\0')]))
        for flag in (OUT,TEXT,TEXT+4,MAIL):cases.append(fixture(1,[flag,TEXT],[*base(),(MAIL,raw+b'\0')]))
        for fn,entry in ((2,0x80252a),(3,0x802c08)):
            cases.append(bounded(fn,[TEXT],[(TEXT,raw+b'\0')],expected=[(0x814688,[entry,0x95000,0x40,0x38,8,0,0x83ccb8])],memory=[(MAIL,raw+b'\0')]))
    # Real frame/border pixels, and independent raster operation oracle.
    for fn in (4,27):
        for depth in (8,32):
            for x,y,w,h in ((8,9,17,12),(3,3,0,0),(8,9,1,1),(8,9,-1,12)):
                if fn==27 and w<0:continue
                args=[7,x,y,w,h,0x35] if fn==4 else [7,x,y,x+w,y+h]
                pixels=bytearray(b'\x7e'*(256*192*4));ops=[]
                if fn==4 and w>=0 and h>=0:
                    r,b=x+w,y+h;ops=[(x-2,y-3,r+1,y-3,15),(x-3,y-3,x-3,b+1,15),(x-3,b+2,r+1,b+2,7),(r+2,y-3,r+2,b+2,7),(x-1,y-2,r,y-2,0),(x-2,y-2,x-2,b,0),(x-2,b+1,r,b+1,8),(r+1,y-2,r+1,b+1,8),(x-1,y-1,r,b,0x35)]
                elif fn==27:
                    for a,b,c,d,color in [(x,y,x+w,y+h,7),(x+1,y+1,x+w-1,y+h-1,15)]:
                        left,right=sorted((a,c));top,bottom=sorted((b,d))
                        ops += [(left,top,right,top,color),(left,bottom,right,bottom,color),(left,top,left,bottom,color),(right,top,right,bottom,color)]
                for a,b,c,d,color in ops:
                    left,right=sorted((a,c));top,bottom=sorted((b,d))
                    for py in range(max(0,top),min(191,bottom)+1):
                        for px in range(max(0,left),min(255,right)+1):
                            off=(py*256+px)*(4 if depth==32 else 1);pixels[off:off+(4 if depth==32 else 1)]=words(color) if depth==32 else bytes([color])
                cases.append(fixture(fn,args,base(depth),memory=[(PIXELS,bytes(pixels))]))
    # Caption routing covers all five themes and unsupported values.
    for style in range(9):
        for active in (0,1,0xffffffff):
            address=0x806742 if style in (4,5) else 0x806228 if style==3 else 0x805e84 if style in (1,2) else 0
            calls=[(address,[7,OBJ+16,active,0x12345678])] if address else []
            cases.append(bounded(5,[7,active],[store(STYLES+8,style)],expected=calls))
    # Explicit external subsystem boundaries retain actual object state/args.
    for owned in (0,1,2):
        for icon in (0,PIXELS):
            calls=[(0x81783e,[7])]+([(0x815978,[icon])] if icon and owned==1 else [])+[(0x815978,[OBJ])]
            cases.append(bounded(11,[7],[store(OBJ+0x567,owned,icon)],expected=calls,memory=[store(0x6a0000+28,0),store(0x1b094,1)]))
    for path in (br'C:\ICON.PNG',b'C:/ICON.PNG',b'ICON.PNG',br'D:\A',b''):
        for icon in (0,PIXELS):
            calls=[(0x827798 if len(path)>2 and path[1:3]==bytes([58,92]) else 0x826f38,[TEXT])]
            cases.append(bounded(12,[7,TEXT,99],[(TEXT,path+b'\0'+bytes(32))],icon,expected=calls,memory=[store(OBJ+0x56b,icon)]))
    for icon in (0,PIXELS,0xffffffff):
        for target in (7,0):cases.append(fixture(13,[target,icon,99],[*base(),store(0x6a0000,OBJ)],memory=[store(OBJ+0x56b,icon)]))
    for fn in (14,15,17,24):
        for visible in (0,1,3):
            for hidden in (0,1,2):
                for active in (0,1,2):
                    setup=[store(SURF+44,visible),store(OBJ+0x123,hidden),store(OBJ+0x10f,active)]
                    cases.append(bounded(fn,[7],setup))
    for fn in (16,17):
        for id in (0,7):
            for active in (0,1,2):
                for registry in (0,1,2):
                    setup=[store(0x6a0000,OBJ),store(OBJ+0x10f,active),store(OBJ+0x127,registry)]
                    cases.append(bounded(fn,[id],setup))
        cases.append(bounded(fn,[8],expected=[]))
    # Actual callback machine code records arguments and mutates its own copy;
    # callbacks also alter object tables so later reloads are observable.
    callback=b'\x8b\x44\x24\x04\xa3'+words(OUT)+b'\x8b\x44\x24\x08\xa3'+words(OUT+4)+b'\x8b\x44\x24\x0c\xa3'+words(OUT+8)+b'\x8b\x44\x24\x10\xa3'+words(OUT+12)+b'\xc7\x44\x24\x04\x11\x22\x33\x44\xc3'
    for fn,field in ((18,0x583),(19,0x58b),(21,0x593),(22,0x597)):
        for pointer in (0,CALLBACK):
            for a,b,c in [(1,2,3),(0,0xffffffff,0x80000000)]:
                # Three-argument callbacks may not read an unpassed fourth.
                body=callback if fn==18 else callback[:27]+callback[36:]
                cases.append(fixture(fn,[7,a,b]+([c] if fn==18 else []),[*base(),store(OBJ+field,pointer),(CALLBACK,body)]))
    for dx,dy in [(0,0),(1,-1),(-20,30),(0x7fffffff,0x80000000)]:
        for blocked in (0,1,0xffffffff):cases.append(bounded(20,[7,dx,dy],[store(0x1b120,blocked)]))
    for count in (0,1,3,8):
        for style in (-1,0,1,3):
            cases.append(bounded(23,[],[store(0x1b098,count),store(OBJ+0x113,style)]))
    for fn in (25,30):
        for id in (0,7,0xffffffff):cases.append(bounded(fn,[id],expected=[(0x81021c if fn==25 else 0x8125dc,[id])]))
    for mode in (0,1,2,3):
        for x in (49,50,51,94,95,108,109,110,139,140,141,153,154,155,198,199,200):
            for y in (119,120,121,146,147,148):
                close=(mode==1 and 95<x<154 or mode==2 and (50<x<109 or 140<x<199)) and 120<y<147
                memory=[store(OBJ+0x563,1)] if mode==2 and 50<x<109 and 120<y<147 else []
                cases.append(bounded(26,[7,x,y],[store(OBJ+0x15b,mode)],expected=[(0x81021c,[7])] if close else [],memory=memory))
    for depth in (0,8,24,32):
        for w,h in ((0,0),(0,3),(4,0),(1,1),(12,9),(-1,1),(1,-1)):
            for color in (0,0xa5,0x12345678,0xffffffff):
                setup=[*base(),store(IMAGE+48,w,h),store(IMAGE+64,depth)]
                memory=[(PIXELS,(bytes([color&255]) if depth==8 else words(color))*max(0,w)*max(0,h))] if depth in (8,32) else []
                cases.append(fixture(29,[7,color],setup,memory=memory))
    for depth in (8,32):
        for off in (28,48,52,64):cases.append(fixture(29,[7,0],[*base(),store(IMAGE+64,depth),store(IMAGE+28,IMAGE+off)]))
    # Subsystem callbacks change live object state between real caller reads.
    for fn in (11,14,15,16,17,20,23,24):
        setup=[store(OBJ+0x123,1),store(OBJ+0x127,1),store(OBJ+0x10f,1)]
        c=bounded(fn,[] if fn==23 else [7,3,4] if fn==20 else [7],setup)
        for address in list(c['boundaries']):
            n,result,_=c['boundaries'][address]
            c['boundaries'][address]=(n,result,[store(OBJ+0x123,2),store(OBJ+12,2),store(OBJ+0x10f,0)])
        cases.append(c)
    # Busy mailbox consumes a byte released asynchronously at the 20th read.
    for fn in (0,2,3):
        c=bounded(fn,[TEXT],[(MAIL,b'busy'+bytes(252))]);c['busy_mailbox']=True;cases.append(c)
    for depth in (8,32):
        for off in (4,8,12):
            args=[7,0];setup=base()+[store(IMAGE+64,depth),store(IMAGE+28,0x40f004+off)]
            c=fixture(29,args,setup);c['caller_alias']=True;cases.append(c)
    cases.append(fixture(29,[8,123],base()))
    return cases

def main():
    p=argparse.ArgumentParser(__doc__);p.add_argument('--installed',action='store_true');a=p.parse_args()
    compare('window_services',SLOTS,Machine,scenarios(),a.installed,['strict signed/wrapped title/client/surface hits','free slot search and topmost order','real strcpy mailbox','independent real 8/32bpp frame/border pixels','real image fill and metadata aliases','exact external boundary arguments and object state','callback caller-copy behavior','all touched pages, ABI and x87 control'])
if __name__=='__main__':main()
