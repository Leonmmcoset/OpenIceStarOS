/* Real original/C/oracle 8-bit rasterization, including forward-copy aliases.
 * Generated ranges are validated before either executable is invoked. */
#include "../kernel/surface_draw.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define PRINT(s) syscall3(4,1,(uint32_t)(s),sizeof(s)-1)
static const uint32_t starts[]={0x1b000,0x500000,0x600000,0x610000,0x620000};
static const uint32_t requests[][4]={
    {0,0,1,1},{2,3,7,4},{0xffffffff,0xfffffffe,9,6},{0,0,0,0},
    {3,4,0,0},{7,5,0xffffffff,0xfffffffe},{12,9,20,20},{17,11,5,4},
    {18,12,1,1},{200,300,1,1},{0x7fffffff,0,1,1},{0,0x7fffffff,1,1},
    {0x80000000,0x80000000,4,3},{1,2,0x80000000,2},
    {1,2,1,0x80000000},{0,0,0xffffffff,0xffffffff},
    {2,2,2,1},{2,2,3,1},{2,2,4,1},{2,2,5,1},{2,2,6,1},{2,2,8,1}};
static const uint32_t layers[][4]={
    {0,0,18,12},{2,3,7,4},{0xfffffffe,0xffffffff,8,5},
    {3,3,0,0},{3,3,0xffffffff,0xfffffffe},{0x7fffffff,0x7fffffff,1,1},
    {1,2,0x80000000,2},{0x80000000,0,0x80000000,1}};
static const uint32_t screens[][2]={{18,12},{0,0},{0x80000000,12},{18,0xffffffff}};
static const uint32_t keys[]={0,1,127,255,256,0x7fffffff,0x80000000,0xffffffff};
static const uint32_t offsets[]={0,1,0xffffffff,4,0xfffffffc};
static uint8_t expected[0x5000];
static uint32_t alignment,alias,rect,layer_case,screen_case,key_case,pattern,metadata;
static uint32_t cases,skipped,coverage,output,input,index;
static int syscall3(int n,uint32_t a,uint32_t b,uint32_t c)
{int r;__asm__ volatile("int $0x80":"=a"(r):"a"(n),"b"(a),"c"(b),"d"(c):"memory");return r;}
static int map(uint32_t address,uint32_t size)
{uint32_t a[]={address,size,7,0x32,0xffffffff,0};return (uint32_t)syscall3(90,(uint32_t)a,0,0)==address;}
static int load(const char *path,uint32_t address)
{int fd=syscall3(5,(uint32_t)path,0,0),n;if(fd<0)return 0;n=syscall3(3,(uint32_t)fd,address,0x40000);syscall3(6,(uint32_t)fd,0,0);return n==250099;}
static void number(uint32_t n)
{char s[10];uint32_t i=10;do{s[--i]='0'+n%10;n/=10;}while(n);syscall3(4,1,(uint32_t)(s+i),10-i);}
static int64_t signed_word(uint32_t n)
{return n&0x80000000u?(int64_t)n-0x100000000LL:n;}
static int valid(uint32_t address,uint32_t length)
{
    uint32_t r;
    if(length>0x1000)return 0;
    for(r=0;r<COUNT(starts);++r)
        if(address>=starts[r] && address<=starts[r]+0x1000-length)return 1;
    return 0;
}
static void initialize(void)
{
    uint32_t r,i;
    for(r=0;r<COUNT(starts);++r)for(i=0;i<0x1000;++i)
        BYTE(starts[r]+i)=(uint8_t)(i*37+pattern*83+r*59);
    output=0x610700+alignment;
    /* Pixel stores may overwrite the live coordinates/key, cached root
     * dimensions, or cached output-base word. Descriptors stay mapped. */
    if(metadata==1)output=0x600101+alignment+4;
    if(metadata==2)output=0x600101+alignment+8;
    if(metadata==3)output=0x600101+alignment+40;
    if(metadata==4)output=0x600001+alignment+12;
    if(metadata==5)output=0x600001+alignment+16;
    if(metadata==6)output=0x1b070;
    input=alias?output+offsets[alias-1]:0x620700+alignment;
    WORD(0x500000)=0x600001+alignment;WORD(0x500004)=0x600101+alignment;
    WORD(0x600001+alignment+12)=screens[screen_case][0];
    WORD(0x600001+alignment+16)=screens[screen_case][1];
    WORD(0x600101+alignment)=input;
    for(i=0;i<4;++i)WORD(0x600101+alignment+4+4*i)=layers[layer_case][i];
    WORD(0x600101+alignment+40)=keys[key_case];WORD(0x1b070)=output;
    index=1+0x40000000u*((pattern+rect)%4);
}
/* Independent oracle uses explicit signed 64-bit comparisons and validates
 * each memory access; it never calls a recovered routine. */
static int oracle(uint32_t *result)
{
    uint32_t x=requests[rect][0],y=requests[rect][1],w=requests[rect][2],h=requests[rect][3];
    uint32_t left=layers[layer_case][0],top=layers[layer_case][1];
    uint32_t width=layers[layer_case][2],height=layers[layer_case][3];
    uint32_t pitch=screens[screen_case][0],screen_height=screens[screen_case][1];
    uint32_t right=left+width-1,bottom=top+height-1,row,rows=0,col,columns;
    uint32_t request_right=x+w-1,request_bottom=y+h-1,span;
    int32_t words,tail;
    if(signed_word(x)>=signed_word(pitch)){*result=x;coverage|=1;return 1;}
    if(signed_word(y)>=signed_word(screen_height)){*result=y;coverage|=2;return 1;}
    if((signed_word(left+width)<=signed_word(x) && signed_word(top+height)<=signed_word(y)) ||
       (signed_word(x+pitch)<=signed_word(left) && signed_word(y+screen_height)<=signed_word(top))){
        *result=0;coverage|=4;return 1;
    }
    if(signed_word(left)<signed_word(x)){left=x;coverage|=8;}
    if(signed_word(top)<signed_word(y)){top=y;coverage|=16;}
    if(signed_word(right)>signed_word(request_right)){right=request_right;coverage|=32;}
    if(signed_word(bottom)>signed_word(request_bottom)){bottom=request_bottom;coverage|=64;}
    span=right-left+1;words=(int32_t)(signed_word(span)/4);
    if(signed_word(span)<0 && span%4)--words;
    tail=(int32_t)(signed_word(span)%4);
    for(row=top;signed_word(row)<=signed_word(bottom);++row){
        if(++rows>24)return 0;
        if(!(keys[key_case]&0x80000000)){
            coverage|=128;columns=0;
            for(col=left;signed_word(col)<=signed_word(right);++col){
                uint32_t relative_x=col-WORD(WORD(0x500000+4*index)+4);
                uint32_t relative_y=row-WORD(WORD(0x500000+4*index)+8);
                uint32_t source=input+relative_y*width+relative_x;
                uint32_t dest=output+row*pitch+col,pixel;
                if(++columns>64 || !valid(source,1))return 0;
                pixel=BYTE(source);
                if(pixel==WORD(WORD(0x500000+4*index)+40)){coverage|=256;continue;}
                if(!valid(dest,1))return 0;
                BYTE(dest)=BYTE(source);coverage|=512;
            }
        }else{
            uint32_t relative_x=left-WORD(WORD(0x500000+4*index)+4);
            uint32_t relative_y=row-WORD(WORD(0x500000+4*index)+8);
            uint32_t source=input+relative_x+relative_y*width;
            uint32_t dest=output+left+row*pitch,n;
            coverage|=1024;
            if(words>64)return 0;
            for(n=0;signed_word(n)<words;++n){
                uint32_t value;
                if(!valid(source+4*n,4)||!valid(dest+4*n,4))return 0;
                value=WORD(source+4*n);WORD(dest+4*n)=value;coverage|=2048;
            }
            for(n=0;signed_word(n)<tail;++n){
                uint32_t delta=4*(uint32_t)words+n;
                if(!valid(source+delta,1)||!valid(dest+delta,1))return 0;
                BYTE(dest+delta)=BYTE(source+delta);coverage|=4096;
            }
        }
    }
    *result=row;return 1;
}
static void save(void)
{uint32_t r,i,n=0;for(r=0;r<COUNT(starts);++r)for(i=0;i<0x1000;++i)expected[n++]=BYTE(starts[r]+i);}
static int compare(void)
{uint32_t r,i,n=0;for(r=0;r<COUNT(starts);++r)for(i=0;i<0x1000;++i)if(expected[n++]!=BYTE(starts[r]+i))return 0;return 1;}
typedef uint32_t (*Draw)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
static int exercise(void)
{
    uint32_t phase,result,expected_result;
    initialize();if(!oracle(&expected_result)){++skipped;return 0;}save();
    for(phase=0;phase<2;++phase){
        Draw draw;
        initialize();
#ifdef KERNEL_LINKED_TEST
        draw=(Draw)(phase?0x817dac:0x917dac);
#else
        draw=phase?kernel_surface_draw8:(Draw)0x917dac;
#endif
        result=draw(index,requests[rect][0],requests[rect][1],requests[rect][2],requests[rect][3]);
        if(result!=expected_result)return 1+phase;
        if(!compare())return 3+phase;
    }
    ++cases;return 0;
}
static int run(void)
{
    uint32_t r,error;
    for(r=0;r<COUNT(starts);++r)if(!map(starts[r],0x1000))return 1;
    if(!map(0x900000,0x40000)||!load("build/icestar.x",0x900000))return 2;
#ifdef KERNEL_LINKED_TEST
    if(!map(0x800000,0x40000)||!load("build/openicestaros_kernel.x",0x800000))return 3;
#endif
    for(pattern=0;pattern<2;++pattern)for(alignment=0;alignment<4;++alignment)
    for(alias=0;alias<6;++alias)for(rect=0;rect<COUNT(requests);++rect)
    for(layer_case=0;layer_case<COUNT(layers);++layer_case)
    for(screen_case=0;screen_case<COUNT(screens);++screen_case)
    for(key_case=0;key_case<COUNT(keys);++key_case){
        error=exercise();if(error){
            PRINT("Surface draw failure error/pattern/alignment/alias/rect/layer/screen/key/cases: ");
            number(error);PRINT("/");number(pattern);PRINT("/");number(alignment);PRINT("/");number(alias);
            PRINT("/");number(rect);PRINT("/");number(layer_case);PRINT("/");number(screen_case);
            PRINT("/");number(key_case);PRINT("/");number(cases);PRINT("\n");return 10+error;
        }
    }
    /* These additional aliases exercise actual writes into metadata, rather
     * than merely moving overlapping input/output pixel buffers. */
    alias=0;
    for(metadata=1;metadata<=6;++metadata)
    for(pattern=0;pattern<2;++pattern)for(alignment=0;alignment<4;++alignment)
    for(rect=0;rect<COUNT(requests);++rect)
    for(layer_case=0;layer_case<COUNT(layers);++layer_case)
    for(screen_case=0;screen_case<COUNT(screens);++screen_case)
    for(key_case=0;key_case<COUNT(keys);++key_case){
        error=exercise();if(error){
            PRINT("Surface metadata failure error/metadata/pattern/alignment/rect/layer/screen/key: ");
            number(error);PRINT("/");number(metadata);PRINT("/");number(pattern);
            PRINT("/");number(alignment);PRINT("/");number(rect);PRINT("/");number(layer_case);
            PRINT("/");number(screen_case);PRINT("/");number(key_case);PRINT("\n");return 10+error;
        }
    }
    if(coverage!=8191){PRINT("Surface draw missing coverage: ");number(coverage);PRINT("\n");return 20;}
    PRINT("8-bit layer rasterizer original/C/oracle cases: ");number(cases);
    PRINT("; unsafe generated ranges skipped: ");number(skipped);
    PRINT("; signed clipping, color key, DWORD/tail overlap, live metadata writes, unaligned descriptors, wrapped indices, raw returns and full memory verified\n");
    return 0;
}
void _start(void){syscall3(1,run(),0,0);__builtin_unreachable();}
