/* Actual window ordering, focus messages, callback and title-button geometry.
 * The title theme fixture is restored before drawing/closing the window. */
#define fat_guest_entry window_focus_legacy_body
#include "window_services_guest.c"
#undef fat_guest_entry
static uint32_t notices[3],lost_count,lost_id;
static void lost(uint32_t id){++lost_count;lost_id=id;}
void fat_guest_entry(void)
{
    emit(0x57464331);
    uint32_t a=CALL(0x807a0a,100,80,260,180,1,0,3,P("Window focus A"));
    uint32_t b=CALL(0x807a0a,160,120,260,180,1,0,3,P("Window focus B"));
    emit((int32_t)a>0 && (int32_t)b>0 && a!=b);
    uint32_t ao=D(0x6a0000+a*4),bo=D(0x6a0000+b*4);
    D(ao+0x55f)=P(notices);D(ao+0x5a7)=P(lost);
    CALL(0x8082c6,a);emit(D(ao+0x10f)==1 && D(0x1b0a0)==a && notices[0]==1);
    CALL(0x8082c6,b);emit(D(ao+0x10f)==0 && notices[1]==1 && lost_count==1 && lost_id==a);
    CALL(0x8084a0,a);emit(D(ao+0x10f)==0 && D(ao+0x123)==0);
    notices[0]=0;CALL(0x8087e0,a);emit(D(ao+0x10f)==1 && D(0x1b0a0)==a && notices[0]==1);
    notices[0]=0;CALL(0x8087e0,a);emit(notices[0]==0);
    CALL(0x808b78,b);emit(D(bo+0x10f)==0);
    uint32_t styles=D(0x1b178),oldtheme=D(styles+4),oldmode=D(ao+0x11b);
    for(uint32_t theme=1;theme<=5;++theme){
        D(styles+4)=theme;D(ao+0x11b)=3;
        uint32_t shift=theme>=4?3:0,cy=theme>=4?90:88;
        emit(CALL(0x808db8,a,344-shift,cy));
        emit(CALL(0x808db8,a,310-shift,cy));
        emit(CALL(0x808db8,a,328-shift,cy));
        emit(CALL(0x808db8,a,336-shift,cy));
        D(ao+0x11b)=2;emit(CALL(0x808db8,a,328-shift,cy));
    }
    D(styles+4)=oldtheme;D(ao+0x11b)=oldmode;
    CALL(0x81021c,a);emit(notices[2]==1 && D(0x6a0000+a*4)==0);
    CALL(0x81021c,b);emit(D(0x6a0000+b*4)==0);
    window_focus_legacy_body();
}
