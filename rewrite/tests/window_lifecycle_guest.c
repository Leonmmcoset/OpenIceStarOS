/* Extend the existing real guest lifecycle with all four style selectors,
 * screen-sized backing and real close callback/completion notification. */
#define fat_guest_entry window_services_guest_body
#include "window_services_guest.c"
#undef fat_guest_entry
static uint32_t closing_count,closing_valid,completion[3];
static void closing(uint32_t id)
{
    ++closing_count;
    closing_valid=D(0x6a0000+id*4)!=0 && completion[2]==0;
}
void fat_guest_entry(void)
{
    emit(0x574c4332);
    for(uint32_t style=0;style<4;++style){
        uint32_t w=CALL(0x807a0a,11+style,22,120,80,style,0,0,P("Window lifecycle"));
        emit((int32_t)w>0);
        uint32_t obj=D(0x6a0000+w*4),surf=D(0x500000+D(obj+12)*4);
        emit(D(surf+4)==11+style && D(surf+8)==22 && D(surf+12)==120 && D(surf+16)==80);
        closing_count=0;closing_valid=0;completion[2]=0;
        D(obj+0x5b7)=P(closing);D(obj+0x55f)=P(completion);
        CALL(0x81021c,w);
        emit(D(0x6a0000+w*4)==0 && closing_count==1 && closing_valid==1 && completion[2]==1);
    }
    uint32_t w=CALL(0x807a0a,11,22,120,80,1,0,3,P("Screen backing"));emit((int32_t)w>0);
    uint32_t obj=D(0x6a0000+w*4),surf=D(0x500000+D(obj+12)*4);
    emit(D(surf+12)==120 && D(surf+16)==80 && D(surf+20)==1024 && D(surf+24)==768);
    CALL(0x81021c,w);emit(D(0x6a0000+w*4)==0);
    window_services_guest_body();
}
