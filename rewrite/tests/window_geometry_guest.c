/* Real heap/frame/compositor callbacks. Taskbar orientation/dimensions are
 * temporary guest fixtures restored before the existing lifecycle diagnostic. */
#define fat_guest_entry window_geometry_legacy_body
#include "window_services_guest.c"
#undef fat_guest_entry
static uint32_t redraw_count,redraw_id,redraw_state;
static void geometry_redraw(uint32_t id)
{
    ++redraw_count;redraw_id=id;redraw_state=D(D(0x6a0000+id*4)+0x11f);
}
void fat_guest_entry(void)
{
    emit(0x57474d31);
    uint32_t id=CALL(0x807a0a,90,70,120,80,1,0,0,P("Window geometry"));emit((int32_t)id>0);
    uint32_t obj=D(0x6a0000+id*4),surface=D(0x500000+D(obj+12)*4);
    D(obj+0x11b)=3;D(obj+0x573)=P(geometry_redraw);
    uint32_t saved=lock();
    uint32_t bar=D(0x500000+D(0x1b0a4)*4),oldw=D(bar+12),oldh=D(bar+16),oldpos=D(0x1b09c);
    D(bar+12)=32;D(bar+16)=28;
    for(uint32_t pos=0;pos<6;++pos){
        D(0x1b09c)=pos;redraw_count=0;
        CALL(0x80ed56,id);
        emit(D(obj+0x11b)==4 && D(obj+0x11f)==1 && redraw_count==1 && redraw_id==id && redraw_state==1);
        emit(D(surface+4));emit(D(surface+8));emit(D(surface+12));emit(D(surface+16));
        emit(D(surface+20)==1024 && D(surface+24)==768 && D(surface)!=0);
        /* Isolate this layout walk from other apps' resize callbacks, which
         * legitimately rewrite taskbar dimensions for each orientation. */
        uint32_t oldzero=D(0x6a0000),oldcount=D(0x1b098);
        D(0x6a0000)=obj;D(0x1b098)=1;CALL(0x80f4be);
        D(0x6a0000)=oldzero;D(0x1b098)=oldcount;
        emit(redraw_count==2 && redraw_id==0 && redraw_state==1);
        CALL(0x80fbfc,id);
        emit(D(surface+4)==90 && D(surface+8)==70 && D(surface+12)==120 && D(surface+16)==80);
        emit(D(obj+0x11b)==3 && D(obj+0x11f)==0 && redraw_count==3 && redraw_state==0);
    }
    D(bar+12)=oldw;D(bar+16)=oldh;D(0x1b09c)=oldpos;flags(saved);
    CALL(0x81021c,id);emit(D(0x6a0000+id*4)==0);
    window_geometry_legacy_body();
}
