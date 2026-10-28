/* Original x86/C/independent-oracle rasterization into 32-bit output.
 * Observed row/group callbacks retain separate outgoing argument areas. */
#include "../kernel/surface_draw.h"
#include "../kernel/surface_alpha.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define PRINT(s) syscall3(4,1,(uint32_t)(s),sizeof(s)-1)
static const uint32_t starts[]={0x1b000,0x500000,0x600000,0x610000,0x620000,0x630000};
static const uint32_t requests[][4]={
    {0,0,1,1},{2,3,7,4},{0xffffffff,0xfffffffe,9,6},{0,0,0,0},
    {3,4,0,0},{7,5,0xffffffff,0xfffffffe},{12,9,20,20},{17,11,5,4},
    {18,12,1,1},{200,300,1,1},{0x7fffffff,0,1,1},{0,0x7fffffff,1,1},
    {0x80000000,0x80000000,4,3},{1,2,0x80000000,2},
    {1,2,1,0x80000000},{0,0,0xffffffff,0xffffffff},
    {2,2,2,1},{2,2,3,1},{2,2,4,1},{2,2,5,1},{2,2,6,1},{2,2,8,1}};
static const uint32_t layers[][4]={
    {0,0,18,12},{2,3,7,4},{0xfffffffe,0xffffffff,8,5},{3,3,0,0},
    {3,3,0xffffffff,0xfffffffe},{0x7fffffff,0x7fffffff,1,1},
    {1,2,0x80000000,2},{0x80000000,0,0x80000000,1}};
static const uint32_t screens[][2]={{18,12},{0,0},{0x80000000,12},{18,0xffffffff}};
static const uint32_t keys[]={0,1,255,256,0x7fffffff,0x80000000,0xffffffff};
static const uint32_t depths[]={32,8,16};
static const uint32_t offsets[]={0,1,0xffffffff,4,0xfffffffc};
static const uint32_t alphas[]={0,1,64,127,128,255};
static uint8_t expected[0x6000];
static uint32_t events[128][7],expected_events[128][7],expected_after[6];
static uint32_t alignment,alias,rect,layer_case,screen_case,key_case,pattern,metadata,mode,depth_case;
static uint32_t cases,skipped,coverage,output,input,index,object,palette,key,calls,failure;
uint32_t draw32_arguments[5],draw32_after[6];
volatile uint32_t *draw32_caller_arguments;
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
{uint32_t r;if(length>0x1000)return 0;for(r=0;r<COUNT(starts);++r)if(address>=starts[r] && address<=starts[r]+0x1000-length)return 1;return 0;}
static uint32_t get(uint32_t address)
{uint32_t n=0,i;for(i=0;i<4;++i)n|=(uint32_t)BYTE(address+i)<<(8*i);return n;}
static void put(uint32_t address,uint32_t n)
{uint32_t i;for(i=0;i<4;++i)BYTE(address+i)=(uint8_t)(n>>(8*i));}
static void effects(void)
{
    if(calls==1){
        if(mode&1){
            WORD(0x1b0e4)=0xdeadbeef;WORD(0x600301+alignment+68)=0xdeadbeef;
            WORD(0x600301+alignment+72)=0xdeadbeef;WORD(0x500000)=0xdeadbeef;
            WORD(0x1b070)=0xdeadbeef;WORD(0x1b0e0)=0xdeadbeef;
            WORD(object)=0xdeadbeef;WORD(object+12)=0xdeadbeef;WORD(object+16)=0xdeadbeef;
            WORD(object+4)+=1;WORD(object+8)+=1;WORD(object+40)=0xffffffff;
        }
        if(mode&2){
            draw32_caller_arguments[0]+=1;draw32_caller_arguments[1]+=123;
            draw32_caller_arguments[2]^=0x80000000;
            draw32_caller_arguments[3]=0x12345678;draw32_caller_arguments[4]=0xabcdef01;
        }
    }
    WORD(0x1b300)^=calls*0x132457;
}
static void event(uint32_t kind,uint32_t d,uint32_t s,const uint32_t *w,uint32_t n)
{uint32_t i;if(calls>=128){failure=1;return;}events[calls][0]=kind;events[calls][1]=d;events[calls][2]=s;for(i=0;i<4;++i)events[calls][i+3]=kind?w[i]:(i?0:n);++calls;}
__attribute__((used,noinline)) static uint32_t observe_copy(volatile uint32_t *a)
{
    uint32_t d=a[0],s=a[1],n=a[2],i;
    if(n>64){failure=1;return 0;}event(0,d,s,0,n);
    for(i=0;i<n;++i)WORD(d+4*i)=WORD(s+4*i);
    effects();if(mode&4)for(i=0;i<3;++i)a[i]=0xa5a50000|i;
    return 0xcdef0000|calls;
}
__attribute__((used,noinline)) static uint32_t observe_blend(volatile uint32_t *a)
{
    uint32_t d=a[0],s=a[1],w=a[2],i;
    event(1,d,s,(const uint32_t *)(uintptr_t)w,0);
    kernel_surface_blend4(d,s,w);effects();
    if(mode&4){for(i=0;i<4;++i)WORD(w+4*i)=0xdeadbeef;for(i=0;i<3;++i)a[i]=0xa5a50000|i;}
    return 0xcdef0000|calls;
}
__attribute__((naked,noinline)) static uint32_t copier(void)
{__asm__("leal 4(%esp),%eax;pushl %eax;call observe_copy;addl $4,%esp;ret");}
__attribute__((naked,noinline)) static uint32_t blender(void)
{__asm__("leal 4(%esp),%eax;pushl %eax;call observe_blend;addl $4,%esp;ret");}
__attribute__((naked,noinline)) static uint32_t invoke(uint32_t target __attribute__((unused)))
{
    __asm__("pushl %ebp;movl %esp,%ebp;subl $40,%esp;"
        "movl draw32_arguments,%edx;movl %edx,0(%esp);"
        "movl draw32_arguments+4,%edx;movl %edx,4(%esp);"
        "movl draw32_arguments+8,%edx;movl %edx,8(%esp);"
        "movl draw32_arguments+12,%edx;movl %edx,12(%esp);"
        "movl draw32_arguments+16,%edx;movl %edx,16(%esp);"
        "movl $0x93fedc71,20(%esp);movl %esp,draw32_caller_arguments;call *8(%ebp);"
        "movl 0(%esp),%edx;movl %edx,draw32_after;"
        "movl 4(%esp),%edx;movl %edx,draw32_after+4;"
        "movl 8(%esp),%edx;movl %edx,draw32_after+8;"
        "movl 12(%esp),%edx;movl %edx,draw32_after+12;"
        "movl 16(%esp),%edx;movl %edx,draw32_after+16;"
        "movl 20(%esp),%edx;movl %edx,draw32_after+20;leave;ret");
}
static void initialize(void)
{
    uint32_t r,i;
    for(r=0;r<COUNT(starts);++r)for(i=0;i<0x1000;++i)BYTE(starts[r]+i)=(uint8_t)(i*37+pattern*83+r*59);
    output=0x610700+alignment;object=0x600101+alignment;palette=0x630001+alignment;
    if(metadata==1)output=object+4;
    if(metadata==2)output=object+8;
    if(metadata==3)output=object+40;
    if(metadata==4)output=0x600001+alignment+12;
    if(metadata==5)output=0x600001+alignment+16;
    if(metadata==6)output=0x1b070;
    if(metadata==7)output=palette+80;
    input=alias?output+offsets[alias-1]:0x620700+alignment;
    for(i=0;i<192;++i)BYTE(input+4*i+3)=(uint8_t)alphas[(i+pattern)%COUNT(alphas)];
    WORD(0x500000)=0x600001+alignment;WORD(0x500004)=object;WORD(0x500008)=object+0x100;
    WORD(0x600001+alignment+12)=screens[screen_case][0];WORD(0x600001+alignment+16)=screens[screen_case][1];
    for(r=0;r<2;++r){
        WORD(object+0x100*r)=input;
        for(i=0;i<4;++i)WORD(object+0x100*r+4+4*i)=layers[layer_case][i];
        WORD(object+0x100*r+36)=depths[depth_case];
    }
    key=key_case<COUNT(keys)?keys[key_case]:(depths[depth_case]==8?BYTE(input):WORD(input));
    WORD(object+40)=key;WORD(object+0x100+40)=key;
    WORD(0x1b070)=output;WORD(0x1b0e0)=palette;WORD(0x1b0e4)=0x600301+alignment;
    WORD(0x600301+alignment+68)=(uint32_t)copier;WORD(0x600301+alignment+72)=(uint32_t)blender;
    index=1+0x40000000u*((pattern+rect)%4);draw32_arguments[0]=index;
    for(i=0;i<4;++i)draw32_arguments[i+1]=requests[rect][i];
    calls=failure=0;for(i=0;i<128*7;++i)((uint32_t *)events)[i]=0;
}
/* The oracle decodes bytes and performs blending with explicit 64-bit
 * arithmetic. It validates generated ranges before executable invocation. */
static int blend_pixels(uint32_t d,uint32_t s,const uint32_t *weights,uint32_t n)
{
    uint32_t p;
    for(p=0;p<n;++p){
        uint32_t w=weights[p];int32_t c;
        if(signed_word(w)>127){coverage|=1024;continue;}
        if(!valid(s+4*p,4)||!valid(d+4*p,4))return 0;
        if(!w){uint32_t copy=get(s+4*p);put(d+4*p,copy);coverage|=2048;continue;}
        coverage|=4096;
        for(c=2;c>=0;--c){
            uint32_t off=4*p+(uint32_t)c;
            uint64_t sum=(uint64_t)BYTE(d+off)*w+(uint64_t)BYTE(s+off)*(128-w);
            BYTE(d+off)=(uint8_t)(sum/128);
        }
    }
    return 1;
}
static int oracle(uint32_t *result)
{
    uint32_t args[6],i,rows=0,cols,groups=0,tail=0;
    uint32_t x=requests[rect][0],y=requests[rect][1],w=requests[rect][2],h=requests[rect][3];
    uint32_t left=layers[layer_case][0],top=layers[layer_case][1],width=layers[layer_case][2],height=layers[layer_case][3];
    uint32_t pitch=screens[screen_case][0],sh=screens[screen_case][1],right=left+width-1,bottom=top+height-1;
    uint32_t rr=x+w-1,rb=y+h-1,row,span,source_row=0,dest_row=0;
    for(i=0;i<5;++i)args[i]=draw32_arguments[i];
    args[5]=0x93fedc71;draw32_caller_arguments=args;
    if(signed_word(x)>=signed_word(pitch)){*result=x;coverage|=1;goto done;}
    if(signed_word(y)>=signed_word(sh)){*result=y;coverage|=2;goto done;}
    if((signed_word(left+width)<=signed_word(x)&&signed_word(top+height)<=signed_word(y))||
       (signed_word(x+pitch)<=signed_word(left)&&signed_word(y+sh)<=signed_word(top))){*result=0;coverage|=4;goto done;}
    if(signed_word(left)<signed_word(x)){left=x;coverage|=8;}
    if(signed_word(top)<signed_word(y)){top=y;coverage|=16;}
    if(signed_word(right)>signed_word(rr)){right=rr;coverage|=32;}
    if(signed_word(bottom)>signed_word(rb)){bottom=rb;coverage|=64;}
    if(depths[depth_case]==16){*result=object;coverage|=128;goto done;}
    span=right-left+1;
    if(depths[depth_case]==32 && !(key&0x80000000)){
        int64_t q=signed_word(span)/4;
        if(signed_word(span)<0 && span%4)--q;
        groups=(uint32_t)q;tail=(uint32_t)(signed_word(span)%4);
        source_row=input+4*(left-WORD(object+4)+(top-WORD(object+8))*width);
        dest_row=output+4*(top*pitch+left);
    }
    for(row=top;signed_word(row)<=signed_word(bottom);++row){
        if(++rows>24)return 0;
        if(depths[depth_case]==8){
            uint32_t col;cols=0;coverage|=256;
            for(col=left;signed_word(col)<=signed_word(right);++col){
                uint32_t obj=get(0x500000+4*args[0]),rx=col-get(obj+4),ry=row-get(obj+8);
                uint32_t s=input+ry*width+rx,d=output+4*(row*pitch+col),v;
                if(++cols>64||!valid(s,1))return 0;
                if(!(key&0x80000000)&&BYTE(s)==key){coverage|=8192;continue;}
                v=BYTE(s);if(!valid(palette+4*v,4)||!valid(d,4))return 0;
                put(d,get(palette+4*v));
            }
        }else if(key&0x80000000){
            uint32_t obj=get(0x500000+4*args[0]);
            uint32_t rx=left-get(obj+4),ry=row-get(obj+8);
            uint32_t s=input+4*(ry*width+rx),d=output+4*(row*pitch+left);
            coverage|=512;
            if(span>64)return 0;
            for(i=0;i<span;++i)if(!valid(s+4*i,4)||!valid(d+4*i,4))return 0;
            event(0,d,s,0,span);
            for(i=0;i<span;++i){uint32_t copy=get(s+4*i);put(d+4*i,copy);}
            effects();
        }else{
            uint32_t g,s=source_row,d=dest_row,weights[4],p;
            coverage|=16384;
            if(signed_word(groups)>64)return 0;
            for(g=0;signed_word(g)<signed_word(groups);++g){
                for(p=0;p<4;++p){
                    if(!valid(s+4*p,4))return 0;
                    weights[p]=get(s+4*p)==key?128:BYTE(s+4*p+3);
                    if(weights[p]==128)coverage|=32768;
                }
                event(1,d,s,weights,0);if(!blend_pixels(d,s,weights,4))return 0;
                effects();s+=16;d+=16;
            }
            for(p=0;p<3;++p){
                weights[p]=128;
                if(signed_word(p)<signed_word(tail)){
                    if(!valid(s+4*p,4))return 0;
                    if(get(s+4*p)!=key)weights[p]=BYTE(s+4*p+3);
                }
            }
            if(signed_word(tail)>0)coverage|=65536;
            if(!blend_pixels(d,s,weights,3))return 0;
            source_row+=4*width;dest_row+=4*pitch;
        }
    }
    *result=row;
 done:
    for(i=0;i<6;++i)expected_after[i]=args[i];
    return !failure;
}
static void save(void)
{uint32_t r,i,n=0;for(r=0;r<COUNT(starts);++r)for(i=0;i<0x1000;++i)expected[n++]=BYTE(starts[r]+i);for(i=0;i<128*7;++i)((uint32_t *)expected_events)[i]=((uint32_t *)events)[i];}
static int compare(uint32_t expected_calls)
{uint32_t r,i,n=0;if(failure||calls!=expected_calls)return 0;for(r=0;r<COUNT(starts);++r)for(i=0;i<0x1000;++i)if(expected[n++]!=BYTE(starts[r]+i))return 0;for(i=0;i<128*7;++i)if(((uint32_t *)expected_events)[i]!=((uint32_t *)events)[i])return 0;for(i=0;i<6;++i)if(expected_after[i]!=draw32_after[i])return 0;return 1;}
static int exercise(void)
{
    uint32_t phase,result,expected_result,expected_calls,target;
    initialize();if(!oracle(&expected_result)){++skipped;return 0;}expected_calls=calls;save();
    for(phase=0;phase<2;++phase){
        initialize();
#ifdef KERNEL_LINKED_TEST
        target=phase?0x81846a:0x91846a;
#else
        target=phase?(uint32_t)kernel_surface_draw32:0x91846a;
#endif
        result=invoke(target);if(result!=expected_result)return 1+phase;
        if(!compare(expected_calls))return 3+phase;
    }
    ++cases;return 0;
}
static int checked(void)
{
    uint32_t error=exercise();if(error){
        PRINT("Surface draw32 failure error/pattern/alignment/alias/rect/layer/screen/key/depth/mode/metadata/cases: ");
        number(error);PRINT("/");number(pattern);PRINT("/");number(alignment);PRINT("/");number(alias);
        PRINT("/");number(rect);PRINT("/");number(layer_case);PRINT("/");number(screen_case);
        PRINT("/");number(key_case);PRINT("/");number(depth_case);PRINT("/");number(mode);
        PRINT("/");number(metadata);PRINT("/");number(cases);PRINT("\n");return 10+error;
    }return 0;
}
static int run(void)
{
    uint32_t r,error;
    for(r=0;r<COUNT(starts);++r)if(!map(starts[r],0x1000))return 1;
    if(!map(0x900000,0x40000)||!load("build/icestar.x",0x900000))return 2;
#ifdef KERNEL_LINKED_TEST
    if(!map(0x800000,0x40000)||!load("build/openicestaros_kernel.x",0x800000))return 3;
#endif
    for(alignment=0;alignment<4;++alignment)for(alias=0;alias<6;++alias)
    for(rect=0;rect<COUNT(requests);++rect)for(layer_case=0;layer_case<COUNT(layers);++layer_case)
    for(key_case=0;key_case<8;++key_case)for(depth_case=0;depth_case<3;++depth_case)
        if((error=checked()))return error;
    alias=0;
    for(mode=1;mode<8;++mode)for(pattern=0;pattern<2;++pattern)
    for(alignment=0;alignment<4;alignment+=3)for(rect=0;rect<COUNT(requests);++rect)
    for(layer_case=0;layer_case<COUNT(layers);++layer_case)for(screen_case=0;screen_case<4;++screen_case)
    for(key_case=0;key_case<8;++key_case)for(depth_case=0;depth_case<3;++depth_case)
        if((error=checked()))return error;
    mode=0;pattern=1;screen_case=0;
    for(metadata=1;metadata<=7;++metadata)for(alignment=0;alignment<4;++alignment)
    for(rect=0;rect<COUNT(requests);++rect)for(layer_case=0;layer_case<4;++layer_case)
    for(key_case=0;key_case<8;++key_case)for(depth_case=0;depth_case<3;++depth_case)
        if((error=checked()))return error;
    if(coverage!=131071){PRINT("Surface draw32 missing coverage: ");number(coverage);PRINT("\n");return 20;}
    PRINT("32-bit output rasterizer original/C/oracle cases: ");number(cases);
    PRINT("; unsafe generated ranges skipped: ");number(skipped);
    PRINT("; 8/32-bit sources, palette/key/alpha, signed clipping, pixel/metadata aliases, cached callbacks, live layer mutation, outgoing isolation, raw returns and full memory verified\n");
    return 0;
}
void _start(void){syscall3(1,run(),0,0);__builtin_unreachable();}
