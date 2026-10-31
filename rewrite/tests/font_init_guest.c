/* Test-only X: real Fira plus non-CID/CID CFF and partial failure state. */
#include <stdint.h>
#define P(a) ((uint32_t)(uintptr_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(a))
typedef uint32_t (*one_fn)(uint32_t);
typedef uint32_t (*three_fn)(uint32_t,uint32_t,uint32_t);
typedef void (*no_fn)(void);
static uint32_t info[48];
static uint8_t font[4096];
static const char tags[6][4]={{'c','m','a','p'},{'h','e','a','d'},
    {'h','h','e','a'},{'h','m','t','x'},{'m','a','x','p'},{'C','F','F',' '}};
static const uint16_t offsets[]={512,576,640,704,768,1024};
static const uint8_t one_index[]={0,1,1,1,2,11};
static void emit(uint32_t value)
{
    for(uint32_t i=0;i<4;++i){
        __asm__ volatile("outb %0, %1"::"a"((uint8_t)value),"Nd"((uint16_t)0xe9));
        value>>=8;
    }
}
static void b16(uint32_t at,uint32_t value)
{
    font[at]=(uint8_t)(value>>8);font[at+1]=(uint8_t)value;
}
static void b32(uint32_t at,uint32_t value)
{
    b16(at,value>>16);b16(at+2,value);
}
static void integer(uint32_t at,uint32_t value)
{
    font[at]=29;b32(at+1,value);
}
static void synthetic(uint32_t cid,uint32_t kind,uint32_t missing_head)
{
    for(uint32_t i=0;i<4096;++i)font[i]=0;
    for(uint32_t i=0;i<48;++i)info[i]=0xa5a5a5a5;
    b32(0,0x10000);b16(4,6);
    for(uint32_t i=0;i<6;++i){
        for(uint32_t j=0;j<4;++j)font[12+i*16+j]=(uint8_t)tags[i][j];
        b32(20+i*16,offsets[i]);b32(24+i*16,64);
    }
    if(missing_head)font[28]='X';
    b16(514,1);b16(516,3);b16(518,1);b32(520,32);
    b16(626,1);b16(772,17);
    font[1024]=1;font[1026]=4;font[1027]=4;
    for(uint32_t i=0;i<6;++i)font[1028+i]=one_index[i];
    font[1033]='A';
    b16(1034,1);font[1036]=1;font[1037]=1;font[1038]=28;
    integer(1039,64);font[1044]=17;
    integer(1045,kind);font[1050]=12;font[1051]=6;
    integer(1052,cid?96:0);font[1057]=12;font[1058]=36;
    integer(1059,cid?120:0);font[1064]=12;font[1065]=37;
    for(uint32_t i=0;i<6;++i)font[1068+i]=one_index[i];
    for(uint32_t i=0;i<6;++i)font[1088+i]=one_index[i];
    font[1093]=14;
    b16(1120,1);font[1122]=1;font[1123]=1;font[1124]=1;
    emit(((three_fn)0x82c3bc)(P(info),P(font),0));
    emit(info[3]);emit(info[4]);emit(info[5]);emit(info[6]);emit(info[7]);emit(info[8]);
    emit(info[11]);emit(info[12]);emit(info[13]);
    for(uint32_t i=14;i<32;i+=3){
        emit(info[i]==0xa5a5a5a5?info[i]:info[i]?info[i]-P(font):0);
        emit(info[i+1]);emit(info[i+2]);
    }
}
void fat_guest_entry(void)
{
    emit(0x494e4931);
    uint32_t original=((one_fn)0x827798)(P("C:\\ICESTAR\\FONT\\FIRA.TTF"));
    emit(original!=0);emit(((three_fn)0x82c3bc)(P(info),original,0));
    for(uint32_t i=3;i<14;++i)emit(info[i]);
    emit(info[14]);emit(info[15]);emit(info[16]);
    synthetic(0,2,0);synthetic(1,2,0);synthetic(0,1,0);synthetic(0,2,1);
    for(;;)((no_fn)(uintptr_t)D(D(0x1b0e4u)+108u*4u))();
}
