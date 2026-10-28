/* Original and C decode through actual fixed-address pixel/quantizer bodies.
 * The model uses bytewise reads and widened, explicitly wrapped addresses. */
#include "raster_fixture.h"
#include "../kernel/bitmap.h"
static uint32_t surface,bitmap_base,alignment,paths[4],writes,bitmap_stores,
    metadata_stores,caller_stores,changed_caller,changed_bitmap,skipped_pixels,
    pair_reloads,odd_pairs,read_faults;
static uint8_t globals_initial[4096];
static int valid(uint32_t address,uint32_t bytes)
{
    for(unsigned i=0;i<bytes;++i)if(index_of(address+i)==UINT32_MAX)return 0;
    return 1;
}
static uint32_t read_bytes(uint32_t address,unsigned bytes)
{
    uint32_t value=0;
    for(unsigned i=0;i<bytes;++i){REQUIRE(valid(address+i,1),"bitmap oracle read");value|=(uint32_t)expected[index_of(address+i)]<<(8u*i);}
    return value;
}
static uint32_t incoming(unsigned i){return get32(expected,ARGUMENTS+i*4u);}
static int write_bytes(uint32_t address,uint32_t color,unsigned bytes)
{
    if(!valid(address,bytes))return 0;
    REQUIRE(put(expected,address,color,bytes),"bitmap oracle store");++writes;
    if(address-bitmap_base<4096u)++bitmap_stores;
    if(address-DATA<0x400u||address-0x500000u<4096u)++metadata_stores;
    if(address-ARGUMENTS<24u)++caller_stores;
    return 1;
}
static int record(uint32_t id,uint32_t *out)
{
    uint32_t slot=(uint32_t)(0x500000ULL+(uint64_t)id*4u);
    if(!valid(slot,4))return 0;
    *out=read_bytes(slot,4);return 1;
}
static int pixel(uint32_t id,uint32_t x,uint32_t y,uint32_t color,unsigned bytes)
{
    uint32_t s;
    if(signed32(x)<0){++skipped_pixels;return 1;}
    if(!record(id,&s)||!valid(s+12u,4))return 0;
    if(signed32(read_bytes(s+12u,4))<=signed32(x)||signed32(y)<0){++skipped_pixels;return 1;}
    if(!valid(s+16u,4))return 0;
    if(signed32(read_bytes(s+16u,4))<=signed32(y)){++skipped_pixels;return 1;}
    if(!valid(s,4))return 0;
    uint32_t address=(uint32_t)((uint64_t)read_bytes(s,4)+
        ((uint64_t)read_bytes(s+12u,4)*y+x)*bytes);
    return write_bytes(address,color,bytes);
}
static int source_address(uint32_t file,uint32_t header,uint32_t row,uint32_t column,
    unsigned bytes,uint32_t *address)
{
    if(!valid(header+8u,4)||!valid(header+4u,4)||!valid(file+10u,4))return 0;
    uint32_t source_row=read_bytes(header+8u,4)-row-1u;
    *address=(uint32_t)((uint64_t)incoming(3)+read_bytes(file+10u,4)+
        ((uint64_t)source_row*read_bytes(header+4u,4)+column)*bytes);
    return 1;
}
static uint32_t quantize(uint32_t red,uint32_t green,uint32_t blue,uint32_t x,uint32_t y)
{
    static const unsigned matrix[4]={3,1,0,2};unsigned threshold=matrix[(y%2u)*2u+x%2u];
    /* All three components are bytes; the original full-width oracle was
     * separately established for the actual callee. */
    return (((red*21u/256u+threshold)/4u)+6u*((green*21u/256u+threshold)/4u)+
        36u*((blue*21u/256u+threshold)/4u)+16u)%256u;
}
static int model(uint32_t *result)
{
    uint32_t s,x=args[1],y=args[2];
    *result=0x6e25c813u;if(signed32(x)<0)return 1;
    if(!record(args[0],&s)||!valid(s+12u,4))return 0;
    *result=read_bytes(s+12u,4);
    if(signed32(*result)<=signed32(x)||signed32(y)<0)return 1;
    if(!valid(s+16u,4))return 0;
    *result=read_bytes(s+16u,4);if(signed32(*result)<=signed32(y))return 1;
    uint32_t file=args[3],header=file+14u,palette=file+54u;
    if(!valid(header+8u,4)||!valid(header+4u,4)||!valid(s+36u,4))return 0;
    uint32_t h=read_bytes(header+8u,4),w=read_bytes(header+4u,4);
    uint32_t hleft=read_bytes(s+16u,4)-y,wleft=read_bytes(s+12u,4)-x;
    if(h>=hleft)h=hleft;
    if(w>=wleft)w=wleft;
    uint32_t depth=read_bytes(s+36u,4),mode;
    if(depth!=8u&&depth!=32u){*result=s;return 1;}
    if(!valid(header+14u,2))return 0;
    uint32_t bits=read_bytes(header+14u,2);
    if(depth==32u&&(bits==24u||bits==8u||bits==4u))mode=bits==24u?0u:bits==8u?1u:2u;
    else if(depth==8u&&bits==24u)mode=3u;
    else{*result=header;return 1;}
    if((signed32(h)>128)||(signed32(w)>128))return 0;
    uint32_t row;
    for(row=0;signed32(row)<signed32(h);++row)
      for(uint32_t column=0;signed32(column)<signed32(w);column+=mode==2u?2u:1u){
        uint32_t p,color;
        if(!source_address(file,header,row,column,mode==0u||mode==3u?3u:1u,&p))return 0;
        if(mode==0u){if(!valid(p,4))return 0;color=read_bytes(p,4)%0x1000000u;}
        else if(mode==3u){
            if(!valid(p,3))return 0;
            color=quantize(read_bytes(p+2u,1),read_bytes(p+1u,1),read_bytes(p,1),column,row);
        }else{
            if(!valid(p,1))return 0;
            uint32_t index=read_bytes(p,1);if(mode==2u)index%=16u;
            if(!valid(palette+index*4u,4))return 0;
            color=read_bytes(palette+index*4u,4);
        }
        if(!pixel(incoming(0),incoming(1)+column,incoming(2)+row,color,mode==3u?1u:4u))return 0;
        if(mode==2u){
            uint32_t second;
            if(!source_address(file,header,row,column,1u,&second)||!valid(second,1))return 0;
            if(second!=p||read_bytes(second,1)!=initial[index_of(p)])++pair_reloads;
            uint32_t index=read_bytes(second,1)/16u;
            if(!valid(palette+index*4u,4))return 0;
            color=read_bytes(palette+index*4u,4);
            if(!pixel(incoming(0),incoming(1)+column+1u,incoming(2)+row,color,4u))return 0;
            if(column+1u==w)++odd_pairs;
        }
      }
    ++paths[mode];*result=row;return 1;
}
static int oracle(uint32_t *result)
{
    uint32_t *counters[]={&paths[0],&paths[1],&paths[2],&paths[3],&writes,&bitmap_stores,
        &metadata_stores,&caller_stores,&skipped_pixels,&pair_reloads,&odd_pairs};
    uint32_t saved[sizeof(counters)/sizeof(counters[0])];
    for(unsigned i=0;i<sizeof(counters)/sizeof(counters[0]);++i)saved[i]=*counters[i];
    if(model(result))return 1;
    for(unsigned i=0;i<sizeof(counters)/sizeof(counters[0]);++i)*counters[i]=saved[i];
    return 0;
}
static void setup(uint32_t width,uint32_t height,uint32_t depth,uint32_t bits,
    uint32_t image_width,uint32_t image_height,uint32_t destination)
{
    pattern();surface=DATA+0x101u+alignment;bitmap_base=DATA+0x401u+alignment;
    args[3]=bitmap_base;args[4]=random32();args[5]=random32();
    uint32_t offset=(uint32_t)(((uint64_t)width*args[2]+args[1])*(depth==8u?1u:4u));
    REQUIRE(put(initial,0x500000u+args[0]*4u,surface,4)&&put(initial,surface,destination-offset,4)&&
        put(initial,surface+12u,width,4)&&put(initial,surface+16u,height,4)&&put(initial,surface+36u,depth,4),"bitmap surface");
    REQUIRE(put(initial,bitmap_base+10u,0x1000u,4)&&put(initial,bitmap_base+18u,image_width,4)&&
        put(initial,bitmap_base+22u,image_height,4)&&put(initial,bitmap_base+28u,bits,2),"bitmap header");
}
static void exercise(void)
{
    scenario=cases+skipped;begin_oracle();uint32_t value;
    if(!oracle(&value)){++skipped;return;}
    const void *functions[]={(void *)0x81ffc4,(void *)kernel_bitmap_draw_81ffc4,(void *)0x81ffc4};
    for(implementation=0;implementation<3;++implementation){
        REQUIRE(load(implementation?"build/bitmap_candidate.x":"build/bitmap_original.x",0x800000),"bitmap images");
        reset();for(unsigned i=0;i<4096;++i)*(volatile uint8_t *)(0x1b000u+i)=globals_initial[i];
        uint32_t actual[7];REQUIRE(!raster_checked_call(functions[implementation],args,actual),"bitmap ABI");
        compare(actual,value);
        for(unsigned i=0;i<4096;++i)REQUIRE(*(volatile uint8_t *)(0x1b000u+i)==globals_initial[i],"bitmap globals");
    }
    for(unsigned i=0;i<6;++i)if(incoming(i)!=args[i])++changed_caller;
    if(incoming(3)!=args[3])++changed_bitmap;
    ++cases;
}
static void read_guards(void)
{
    stage=9;alignment=0;
    for(unsigned trailing=1;trailing<=3;++trailing){
        args[0]=0;args[1]=0;args[2]=0;setup(1,1,32,24,1,1,DATA+0x1801u);
        REQUIRE(put(initial,bitmap_base+10u,DATA+16384u-trailing-bitmap_base,4),"bitmap overread offset");
        for(implementation=0;implementation<3;++implementation){
            REQUIRE(load(implementation?"build/bitmap_candidate.x":"build/bitmap_original.x",0x800000),"bitmap guard images");
            reset();const void *fn=implementation==1u?(void *)kernel_bitmap_draw_81ffc4:(void *)0x81ffc4;
            int pid=syscall3(2,0,0,0);REQUIRE(pid>=0,"bitmap guard fork");
            if(!pid){uint32_t actual[7];raster_checked_call(fn,args,actual);syscall3(1,0,0,0);}
            uint32_t status=0;REQUIRE(syscall3(7,(uint32_t)pid,(uint32_t)&status,0)==pid,"bitmap guard wait");
            REQUIRE((status&127u)==11u,"original four-byte fetch for 24-bit source");++read_faults;
        }
    }
}
#ifdef BITMAP_CHAIN
void bitmap_direct_start(void)
#else
void _start(void)
#endif
{
    mappings();REQUIRE(map(0x1b000,4096),"bitmap globals map");
    for(unsigned i=0;i<4096;++i)globals_initial[i]=(uint8_t)(i*17u+3u);
    stage=1;static const uint32_t depths[]={0,8,15,16,24,32},bits[]={0,1,4,8,16,24,32,0xffff};
    for(unsigned depth=0;depth<6;++depth)for(unsigned b=0;b<8;++b)
      for(unsigned w=0;w<9;++w)for(unsigned h=0;h<9;++h){
        alignment=(w+h)&3u;args[0]=ids[(w+h+b)%16u];args[1]=1;args[2]=2;
        setup(8,8,depths[depth],bits[b],w,h,DATA+0x2001u+alignment);exercise();
    }
    stage=2;
    for(unsigned mode=0;mode<4;++mode)for(alignment=0;alignment<4;++alignment)
      for(unsigned w=0;w<16;++w)for(unsigned h=0;h<16;++h){
        args[0]=ids[(w+h)%16u];args[1]=values[w];args[2]=values[h];
        setup(values[15u-w],values[15u-h],mode==3u?8u:32u,mode==0u||mode==3u?24u:mode==1u?8u:4u,3,3,DATA+0x2001u+alignment);exercise();
    }
    stage=3;
    for(unsigned mode=0;mode<4;++mode)for(alignment=0;alignment<4;++alignment)
      for(unsigned w=0;w<16;++w)for(unsigned h=0;h<16;++h){
        args[0]=0;args[1]=0;args[2]=0;
        setup(8,8,mode==3u?8u:32u,mode==0u||mode==3u?24u:mode==1u?8u:4u,values[w],values[h],DATA+0x2001u+alignment);exercise();
    }
    stage=4;
    for(unsigned mode=0;mode<4;++mode)for(alignment=0;alignment<4;++alignment)for(unsigned at=0;at<24;++at){
        args[0]=0;args[1]=0;args[2]=0;
        setup(2,2,mode==3u?8u:32u,mode==0u||mode==3u?24u:mode==1u?8u:4u,2,2,ARGUMENTS+at);exercise();
    }
    stage=5;
    for(unsigned mode=0;mode<4;++mode)for(alignment=0;alignment<4;++alignment)for(unsigned alias=0;alias<21;++alias){
        uint32_t base=DATA+0x401u+alignment,s=DATA+0x101u+alignment;
        uint32_t destination=alias<10u?s+alias*4u:alias<13u?0x500000u+(alias-10u)*4u:
            alias<17u?base+10u+(alias-13u)*4u:alias<20u?base+54u+(alias-17u)*4u:base+0x1000u;
        args[0]=0;args[1]=0;args[2]=0;
        setup(2,2,mode==3u?8u:32u,mode==0u||mode==3u?24u:mode==1u?8u:4u,2,2,destination);exercise();
    }
    stage=6;
    for(unsigned mode=0;mode<4;++mode)for(unsigned value=0;value<256;++value){
        alignment=value&3u;args[0]=0;args[1]=0;args[2]=0;
        setup(3,1,mode==3u?8u:32u,mode==0u||mode==3u?24u:mode==1u?8u:4u,3,1,DATA+0x2001u+alignment);
        for(unsigned i=0;i<16;++i)REQUIRE(put(initial,bitmap_base+0x1000u+i,value,1),"bitmap color sweep");
        exercise();
    }
    read_guards();
    REQUIRE(paths[0]&&paths[1]&&paths[2]&&paths[3]&&writes&&bitmap_stores&&metadata_stores&&caller_stores&&changed_caller&&odd_pairs&&read_faults==9,"bitmap coverage");
    PRINT("Bitmap mode scenarios RGB24/palette8/nibbles4/quantized8:");for(unsigned i=0;i<4;++i){PRINT(" ");number(paths[i]);}
    PRINT("; total scenarios/executions/excluded unsafe: ");number(cases);PRINT(" ");number(cases*3u);PRINT(" ");number(skipped);
    PRINT("; writes/bitmap/metadata/caller/changed caller/bitmap argument: ");number(writes);PRINT(" ");number(bitmap_stores);PRINT(" ");number(metadata_stores);PRINT(" ");number(caller_stores);PRINT(" ");number(changed_caller);PRINT(" ");number(changed_bitmap);
    PRINT("; nibble reloads/odd pairs/guard faults: ");number(pair_reloads);PRINT(" ");number(odd_pairs);PRINT(" ");number(read_faults);
    PRINT("; bitmap renderer verified with real pixel/quantizer calls/32 KiB memory/4 KiB globals/caller/EAX/ABI; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
