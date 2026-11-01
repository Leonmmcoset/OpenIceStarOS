/* UTF-8/UTF-16BE name matching, 0x8365b2..0x836f70 and public APIs.
 * Preserve live caller words, unvalidated surrogate pairs and table order. */
#include "font_names.h"
#include "font_parse.h"
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define B(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
uint32_t kernel_font_strlen_82a45d(uint32_t);
int32_t kernel_font_offset_api_836fbc(uint32_t,int32_t);
int32_t kernel_font_name_prefix_8365b2(volatile uint32_t text,volatile int32_t length,
    volatile uint32_t utf16,volatile uint32_t bytes)
{
    uint32_t i=0;
    while(bytes){
        uint16_t c=((uint32_t)B(utf16)<<8)+B(utf16+1u);
        if(c<0x80){
            if((int32_t)i>=length)return -1;
            uint32_t v=B(text+i);++i;if(v!=c)return -1;
        }else if(c<0x800){
            if((int32_t)(i+1u)>=length)return -1;
            uint32_t v=B(text+i);++i;if(v!=0xc0u+(c>>6))return -1;
            v=B(text+i);++i;if(v!=0x80u+(c&63u))return -1;
        }else if(c>=0xd800 && c<=0xdbff){
            uint16_t low=((uint32_t)B(utf16+2u)<<8)+B(utf16+3u);
            if((int32_t)(i+3u)>=length)return -1;
            uint32_t scalar=((uint32_t)(c-0xd800)<<10)+low+0x2400u;
            uint32_t v=B(text+i);++i;if(v!=0xf0u+(scalar>>18))return -1;
            v=B(text+i);++i;if(v!=0x80u+((scalar>>12)&63u))return -1;
            v=B(text+i);++i;if(v!=0x80u+((scalar>>6)&63u))return -1;
            v=B(text+i);++i;if(v!=0x80u+(scalar&63u))return -1;
            utf16+=2u;bytes-=2u;
        }else{
            if(c>=0xdc00 && c<=0xdfff)return -1;
            if((int32_t)(i+2u)>=length)return -1;
            uint32_t v=B(text+i);++i;if(v!=0xe0u+(c>>12))return -1;
            v=B(text+i);++i;if(v!=0x80u+((c>>6)&63u))return -1;
            v=B(text+i);++i;if(v!=0x80u+(c&63u))return -1;
        }
        utf16+=2u;bytes-=2u;
    }
    return (int32_t)i;
}
uint32_t kernel_font_name_equal_83689a(volatile uint32_t text,volatile int32_t length,
    volatile uint32_t utf16,volatile uint32_t bytes)
{
    int32_t n=kernel_font_name_prefix_8365b2(text,length,utf16,bytes);return n==length;
}
uint32_t kernel_font_name_string_8368d8(volatile uint32_t info,volatile uint32_t length,
    volatile uint32_t platform,volatile uint32_t encoding,volatile uint32_t language,
    volatile uint32_t name)
{
    uint32_t data=D(info+4),offset=D(info+8);
    uint32_t table=kernel_font_table_82bfe6(data,offset,0x83d0e7);
    if(!table)return 0;
    uint32_t count=kernel_font_u16_82be14(data+table+2),strings=table+kernel_font_u16_82be14(data+table+4);
    for(uint32_t i=0;i<count;++i){
        uint32_t record=table+6+i*12u;
        if(kernel_font_u16_82be14(data+record)!=platform || kernel_font_u16_82be14(data+record+2)!=encoding ||
            kernel_font_u16_82be14(data+record+4)!=language || kernel_font_u16_82be14(data+record+6)!=name)continue;
        uint32_t output=length;D(output)=kernel_font_u16_82be14(data+record+8);
        uint32_t base=data+strings;
        return base+kernel_font_u16_82be14(data+record+10);
    }
    return 0;
}
uint32_t kernel_font_name_pair_836a34(volatile uint32_t data,volatile uint32_t table,
    volatile uint32_t text,volatile int32_t length,volatile uint32_t first,volatile uint32_t second)
{
    uint32_t count=kernel_font_u16_82be14(data+table+2),strings=table+kernel_font_u16_82be14(data+table+4);
    for(uint32_t i=0;i<count;++i){
        uint32_t record=table+6+i*12u;
        if(kernel_font_u16_82be14(data+record+6)!=first)continue;
        uint32_t platform=kernel_font_u16_82be14(data+record),encoding=kernel_font_u16_82be14(data+record+2),language=kernel_font_u16_82be14(data+record+4);
        if(platform!=0 && !(platform==3 && (encoding==1 || encoding==10)))continue;
        uint32_t n=kernel_font_u16_82be14(data+record+8),off=kernel_font_u16_82be14(data+record+10);
        int32_t matched=kernel_font_name_prefix_8365b2(text,length,data+strings+off,n);
        if(matched<0)continue;
        if(i+1<count && kernel_font_u16_82be14(data+record+18)==second &&
            kernel_font_u16_82be14(data+record+12)==platform && kernel_font_u16_82be14(data+record+14)==encoding && kernel_font_u16_82be14(data+record+16)==language){
            n=kernel_font_u16_82be14(data+record+20);off=kernel_font_u16_82be14(data+record+22);
            if(!n){if(matched==length)return 1;}
            else if(matched<length && B(text+(uint32_t)matched)==' '){
                ++matched;
                if(kernel_font_name_equal_83689a(text+(uint32_t)matched,(int32_t)((uint32_t)length-(uint32_t)matched),data+strings+off,n))return 1;
            }
        }else if(matched==length)return 1;
    }
    return 0;
}
uint32_t kernel_font_match_836cc0(volatile uint32_t data,volatile uint32_t offset,
    volatile uint32_t text,volatile uint32_t flags)
{
    uint32_t length=kernel_font_strlen_82a45d(text);
    if(!kernel_font_signature_82beba(data+offset))return 0;
    if(flags){uint32_t head=kernel_font_table_82bfe6(data,offset,0x83cfa2);
        if((kernel_font_u16_82be14(data+head+44)&7u)!=(flags&7u))return 0;}
    uint32_t names=kernel_font_table_82bfe6(data,offset,0x83d0e7);
    if(!names)return 0;
    if(flags){
        if(kernel_font_name_pair_836a34(data,names,text,length,16,0xffffffff))return 1;
        if(kernel_font_name_pair_836a34(data,names,text,length,1,0xffffffff))return 1;
    }else{
        if(kernel_font_name_pair_836a34(data,names,text,length,16,17))return 1;
        if(kernel_font_name_pair_836a34(data,names,text,length,1,2))return 1;
    }
    return kernel_font_name_pair_836a34(data,names,text,length,3,0xffffffff)!=0;
}
int32_t kernel_font_find_name_836f08(volatile uint32_t data,volatile uint32_t name,volatile uint32_t flags)
{
    for(uint32_t i=0;;++i){
        int32_t offset=kernel_font_offset_api_836fbc(data,(int32_t)i);
        if(offset<0)return offset;
        if(kernel_font_match_836cc0(data,offset,name,flags))return offset;
    }
}
int32_t kernel_font_find_name_api_83700c(uint32_t data,uint32_t name,uint32_t flags)
{return kernel_font_find_name_836f08(data,name,flags);}
uint32_t kernel_font_name_equal_api_83702e(uint32_t text,int32_t length,uint32_t utf16,uint32_t bytes)
{return kernel_font_name_equal_83689a(text,length,utf16,bytes);}
