/* Independent bytewise, bounded models; execute the real service initializer
 * and strlen in the original/compiled images, without helper substitutions. */
#include "raster_fixture.h"
#include "../kernel/string_extra.h"
#define ENVIRONMENT 0x62000u
static uint8_t env_initial[4096],env_expected[4096],globals[4096];
static uint32_t service_table[320];
static uint32_t mode,budget,paths[7],stores,live_stores,env_stores,changed_words,
    cursor_updates,zero_empty,split_tokens,guard_faults,guard_loops,guard_safe;
static const uint32_t entries[]={0x82a531,0x82a57d,0x82a609,0x82a70d,0x82a755,0x82a787,0x82a8a3};
static const void *functions[]={kernel_string_upper_82a531,kernel_string_find_82a57d,
    kernel_config_value_82a609,kernel_string_character_82a70d,kernel_character_upper_82a755,
    kernel_unsigned_text_82a787,kernel_string_split_82a8a3};
static int byte_read(uint32_t a,uint32_t *v)
{
    if(++budget>4096u)return 0;
    uint32_t i=index_of(a);
    if(i!=UINT32_MAX){*v=expected[i];return 1;}
    if(a-ENVIRONMENT<4096u){*v=env_expected[a-ENVIRONMENT];return 1;}
    return 0;
}
static int byte_write(uint32_t a,uint32_t v)
{
    uint32_t i=index_of(a);
    if(i!=UINT32_MAX){expected[i]=(uint8_t)v;++stores;if(a-ARGUMENTS<24u)++live_stores;return 1;}
    if(a-ENVIRONMENT<4096u){env_expected[a-ENVIRONMENT]=(uint8_t)v;++stores;++env_stores;return 1;}
    return 0;
}
static uint32_t arg(unsigned i){return get32(expected,ARGUMENTS+i*4u);}
static int signed_less(uint32_t a,uint32_t b){return signed32(a)<signed32(b);}
static int length_at(uint32_t a,uint32_t *length)
{
    uint32_t ch;*length=0;
    for(;;++*length){if(!byte_read(a+*length,&ch))return 0;if(!ch)return 1;}
}
static uint32_t divide(uint32_t value,uint32_t divisor,uint32_t *remainder)
{
    uint64_t rem=0;uint32_t quotient=0;
    for(unsigned bit=32;bit--;){rem=rem*2u+((value>>bit)&1u);if(rem>=divisor){rem-=divisor;quotient|=1u<<bit;}}
    *remainder=(uint32_t)rem;return quotient;
}
static int model(uint32_t *result)
{
    uint32_t a,b,c,d,i=0,j,n,m;
    if(mode==0){
        for(;;++i){
            if(!byte_read(arg(0)+i,&a))return 0;
            if(!a){*result=arg(0)+i;return 1;}
            if(a>96u&&a<=122u&&!byte_write(arg(0)+i,a-32u))return 0;
        }
    }
    if(mode==1||mode==2){
        uint32_t needle=args[1],start=mode==1?args[0]:0x62200u;
        if(!byte_read(needle,&b))return 0;
        if(!b||(mode==2&&b==32u)){*result=mode==1?start:0u;return 1;}
        for(;;++start){
            if(!byte_read(start,&a))return 0;
            if(!a){*result=0;return 1;}
            if(!byte_read(needle,&b))return 0;
            if(a!=b)continue;
            uint32_t left=start,right=needle;
            for(;;){
                if(!byte_read(left,&a))return 0;
                if(!a)break;
                if(!byte_read(right,&b))return 0;
                if(!b||a!=b)break;
                ++left;++right;
            }
            if(!byte_read(right,&b))return 0;
            if(b)continue;
            if(mode==1){*result=start;return 1;}
            if(!byte_read(left,&a))return 0;
            if(a!=61u)continue;
            for(i=0;i<255u;++i){
                if(!byte_read(left+1u+i,&a))return 0;
                if(a==59u)break;
                if(!byte_write(arg(0)+i,a))return 0;
            }
            if(!byte_write(arg(0)+i,0))return 0;
            *result=1;return 1;
        }
    }
    if(mode==3){
        c=args[1]&255u;
        for(;;){
            if(!byte_read(arg(0),&a))return 0;
            if(!a||a==c)break;
            REQUIRE(put(expected,ARGUMENTS,arg(0)+1u,4),"cursor incoming update");++cursor_updates;
        }
        if(!byte_read(arg(0),&a))return 0;
        *result=a==c?arg(0):0;return 1;
    }
    if(mode==4){
        a=args[0]&255u;
        if(a>96u&&a<=122u)a-=32u;
        *result=a<128u?a:a|0xffffff00u;return 1;
    }
    if(mode==5){
        a=args[0];n=0;
        if(a){
            do{if(++budget>4096u||!arg(2))return 0;a=divide(a,arg(2),&b);++n;}while(a);
            if(signed_less(n,arg(3)))n=arg(3);
            a=args[0];if(!byte_write(arg(1)+n,0))return 0;
            for(--n;!signed_less(n,0);--n){
                if(++budget>4096u||!arg(2))return 0;
                divide(a,arg(2),&b);
                if(!byte_write(arg(1)+n,(b+48u)&255u)||!byte_read(arg(1)+n,&c))return 0;
                if(c>57u&&c<128u&&!byte_write(arg(1)+n,(c+7u)&255u))return 0;
                if(!arg(2))return 0;
                a=divide(a,arg(2),&b);
            }
        }else{
            if(!byte_write(arg(1),48)||!byte_write(arg(1)+1u,0))return 0;
            for(;signed_less(n,arg(3));++n){if(++budget>4096u||!byte_write(arg(1)+n,48))return 0;}
            if(!byte_write(arg(1)+n,0))return 0;
            if(!n)++zero_empty;
        }
        *result=arg(1);return 1;
    }
    if(!length_at(args[2],&n)||!length_at(args[1],&m))return 0;
    for(i=0;signed_less(i,n);++i){
        if(++budget>4096u)return 0;
        uint32_t same=1;
        for(j=0;signed_less(j,m);++j){
            if(!byte_read(arg(1)+j,&a)||!byte_read(arg(2)+i+j,&b))return 0;
            if(a!=b){same=0;break;}
        }
        if(same){
            for(j=0;signed_less(j,m);++j)if(!byte_write(arg(2)+i+j,0))return 0;
            i=i+m-1u;
        }
    }
    c=0;d=0;
    for(i=0;signed_less(i,n);++i){
        if(!byte_read(arg(2)+i,&a))return 0;
        if(!a)d=0;
        else if(!d){
            d=1;b=arg(2)+i;
            uint32_t destination=arg(0)+c*4u;
            for(j=0;j<4;++j)if(!byte_write(destination+j,b>>(j*8u)))return 0;
            ++c;
        }
    }
    split_tokens+=c;*result=c;return 1;
}
static void setup(void)
{
    pattern();
    for(unsigned i=0;i<4096;++i)env_initial[i]=(uint8_t)(i*23u+7u);
    env_initial[512]=0;
    for(unsigned i=0;i<6;++i)args[i]=random32();
}
static void string_at(uint32_t a,const char *s)
{
    do{REQUIRE(put(initial,a++,(uint8_t)*s,1),"fixture string");}while(*s++);
}
static void env_string(const char *s)
{
    unsigned i=512;
    do{env_initial[i++]=(uint8_t)*s;}while(*s++);
}
static void exercise(void)
{
    stage=mode+1u;scenario=cases+skipped;budget=0;begin_oracle();
    for(unsigned i=0;i<4096;++i)env_expected[i]=env_initial[i];
    uint32_t saved[]={stores,live_stores,env_stores,cursor_updates,zero_empty,split_tokens},value;
    if(!model(&value)){
        stores=saved[0];live_stores=saved[1];env_stores=saved[2];cursor_updates=saved[3];zero_empty=saved[4];split_tokens=saved[5];
        ++skipped;return;
    }
    for(implementation=0;implementation<3;++implementation){
        REQUIRE(load(implementation?"build/string_extra_candidate.x":"build/string_extra_original.x",0x800000),"string images");
        *(volatile uint32_t *)0x1b0e4=0x620000;*(volatile uint32_t *)0x1b034=0x610000;
        static const uint32_t depths[]={0,8,15,16,24,32};
        *(volatile uint32_t *)0x610024=depths[scenario%6u];*(volatile uint32_t *)0x1a868=scenario&1u?1u<<25:0;
        for(unsigned i=0;i<320;++i)*(volatile uint32_t *)(0x620000u+i*4u)=i*0x1234567u+cases;
        ((void (*)(void))0x8005aa)();
        for(unsigned i=0;i<7;++i)REQUIRE(*(volatile uint32_t *)(0x620000u+(186u+i)*4u)==entries[i],"string initialized services");
        for(unsigned i=0;i<320;++i){
            uint32_t entry=*(volatile uint32_t *)(0x620000u+i*4u);
            if(!implementation)service_table[i]=entry;
            else REQUIRE(entry==service_table[i],"string initialized table");
        }
        for(unsigned i=0;i<4096;++i)globals[i]=*(volatile uint8_t *)(0x1b000u+i);
        reset();for(unsigned i=0;i<4096;++i)*(volatile uint8_t *)(ENVIRONMENT+i)=env_initial[i];
        const void *fn=implementation==1u?functions[mode]:(void *)(uintptr_t)*(volatile uint32_t *)(0x620000u+(186u+mode)*4u);
        uint32_t actual[7];REQUIRE(!raster_checked_call(fn,args,actual),"string ABI");compare(actual,value);
        for(unsigned i=0;i<320;++i)REQUIRE(*(volatile uint32_t *)(0x620000u+i*4u)==service_table[i],"string service table mutation");
        for(unsigned i=0;i<4096;++i){
            REQUIRE(*(volatile uint8_t *)(ENVIRONMENT+i)==env_expected[i],"environment oracle");
            REQUIRE(*(volatile uint8_t *)(0x1b000u+i)==globals[i],"string globals");
        }
    }
    for(unsigned i=0;i<6;++i)if(arg(i)!=args[i])++changed_words;
    ++paths[mode];++cases;
}
static void guards(void)
{
    stage=9;
    /* Unsupported memory must remain unchecked; radix 0 faults, radix 1
     * and empty separator loop. Zero formatting avoids even radix 0. */
    for(unsigned probe=0;probe<5;++probe){
        mode=probe<2u?0u:probe==4u?6u:5u;setup();args[0]=DATA+100u;
        if(probe<2u){args[0]=DATA+16383u;REQUIRE(put(initial,args[0],probe?0u:97u,1),"upper guard byte");}
        else if(probe==4u){args[0]=DATA+4096u;args[1]=DATA+200u;args[2]=DATA+100u;string_at(args[1],"");string_at(args[2],"a");}
        else{args[0]=1;args[1]=DATA+100u;args[2]=probe-2u;args[3]=0;}
        for(implementation=0;implementation<3;++implementation){
            REQUIRE(load(implementation?"build/string_extra_candidate.x":"build/string_extra_original.x",0x800000),"string guard image");reset();
            int pid=syscall3(2,0,0,0);REQUIRE(pid>=0,"string guard fork");
            if(!pid){
                if(probe>=3u)syscall3(27,1,0,0);
                uint32_t actual[7];raster_checked_call(implementation==1u?functions[mode]:(void *)(uintptr_t)entries[mode],args,actual);
                syscall3(1,probe==1u&&actual[0]==DATA+16383u?0u:1u,0,0);
            }
            uint32_t status=0;REQUIRE(syscall3(7,(uint32_t)pid,(uint32_t)&status,0)==pid,"string guard wait");
            if(probe==1u){REQUIRE(!status,"upper terminator safe");++guard_safe;}
            else{REQUIRE((status&127u)==(probe==0u?11u:probe==2u?8u:14u),"unchecked string/division/loop behavior");if(probe<3u)++guard_faults;else ++guard_loops;}
        }
    }
}
void _start(void)
{
    mappings();REQUIRE(map(0x1a000,8192)&&map(ENVIRONMENT,4096)&&map(0x610000,4096)&&map(0x620000,4096),"string maps");
    mode=0;
    for(unsigned ch=0;ch<256;++ch)for(unsigned alignment=0;alignment<4;++alignment){
        setup();args[0]=DATA+101u+alignment;
        for(unsigned i=0;i<8;++i)REQUIRE(put(initial,args[0]+i,(ch+i*31u)&255u,1),"upper byte");
        REQUIRE(put(initial,args[0]+8u,0,1),"upper NUL");exercise();
    }
    for(unsigned alias=0;alias<24;++alias){setup();args[0]=ARGUMENTS+alias;args[1]=0x61626364u;args[2]=0x7a797800u;exercise();}
    mode=1;
    static const char *haystacks[]={"","a","aaaaab","ababa","abcabc","abcdef"};
    static const char *needles[]={"","a","aa","aba","bc","abcdefg"};
    for(unsigned h=0;h<6;++h)for(unsigned needle=0;needle<6;++needle)for(unsigned offset=0;offset<8;++offset){
        setup();args[0]=DATA+100u+offset;args[1]=DATA+300u;string_at(args[0],haystacks[h]);string_at(args[1],needles[needle]);exercise();
    }
    for(unsigned ch=1;ch<256;++ch){setup();args[0]=DATA+100u;args[1]=DATA+200u;REQUIRE(put(initial,args[0],ch,1)&&put(initial,args[0]+1u,0,1)&&put(initial,args[1],ch,1)&&put(initial,args[1]+1u,0,1),"find byte");exercise();}
    mode=2;
    static const char *names[]={""," ","NAME","AME","NAME=","MISS","OTHER","LONG","ZERO"};
    static const uint32_t lengths[]={0,1,254,255,256,320};
    static const char config_prefix[]="OTHER=old;XNAME=value;NAME=later;LONG=";
    for(unsigned name=0;name<9;++name)for(unsigned alias=0;alias<10;++alias)for(unsigned length=0;length<6;++length){
        setup();args[0]=alias==0u?DATA+1000u:alias<5u?0x62200u+(alias-1u)*4u:ARGUMENTS+alias-5u;
        args[1]=DATA+100u;string_at(args[1],names[name]);env_string(config_prefix);
        unsigned start=512u+sizeof(config_prefix)-1u;
        for(unsigned i=0;i<321;++i)env_initial[start+i]=(uint8_t)('a'+i%26u);
        env_initial[start+lengths[length]]=';';env_initial[start+321u]=0;
        exercise();
    }
    setup();args[0]=DATA+1000u;args[1]=DATA+100u;string_at(args[1],"ZERO");env_string("ZERO=x");
    env_initial[519]='y';env_initial[520]=';';exercise();
    mode=3;
    for(unsigned ch=0;ch<256;++ch)for(unsigned high=0;high<4;++high){
        setup();args[0]=DATA+100u;args[1]=ch|(values[high+12u]&0xffffff00u);
        for(unsigned i=0;i<16;++i)REQUIRE(put(initial,args[0]+i,(ch+17u*i+1u)&255u,1),"character byte");
        REQUIRE(put(initial,args[0]+16u,0,1),"character NUL");exercise();
    }
    mode=4;
    for(unsigned ch=0;ch<256;++ch)for(unsigned high=0;high<16;++high){setup();args[0]=ch|(values[high]&0xffffff00u);exercise();}
    mode=5;
    static const uint32_t bases[]={0,1,2,3,8,10,16,36,64,80,128,255,256,0x80000000,0xfffffffe,0xffffffff};
    static const uint32_t widths[]={0,1,2,7,32,255,0x7fffffff,0x80000000,0xffffffff};
    for(unsigned value=0;value<16;++value)for(unsigned base=0;base<16;++base)for(unsigned width=0;width<9;++width){
        setup();args[0]=values[value];args[1]=DATA+1000u;args[2]=bases[base];args[3]=widths[width];exercise();
    }
    for(unsigned alias=0;alias<24;++alias)for(unsigned value=0;value<5;++value)for(unsigned base=0;base<4;++base){
        setup();args[0]=values[value];args[1]=ARGUMENTS+alias;args[2]=bases[base+2u];args[3]=3;exercise();
    }
    mode=6;
    static const char *separators[]={"a","b","ab","aa","aba","abcdefg",""};
    for(unsigned length=0;length<8;++length)for(unsigned mask=0;mask<(1u<<length);++mask)for(unsigned sep=0;sep<7;++sep){
        setup();args[0]=DATA+1000u;args[1]=DATA+100u;args[2]=DATA+300u;string_at(args[1],separators[sep]);
        for(unsigned i=0;i<length;++i)REQUIRE(put(initial,args[2]+i,mask&(1u<<i)?'a':'b',1),"split byte");
        REQUIRE(put(initial,args[2]+length,0,1),"split NUL");exercise();
    }
    for(unsigned alias=0;alias<42;++alias)for(unsigned sep=0;sep<6;++sep){
        setup();args[0]=alias<24u?ARGUMENTS+alias:alias<32u?DATA+300u+alias-24u:DATA+100u+alias-32u;
        args[1]=DATA+100u;args[2]=DATA+300u;string_at(args[1],separators[sep]);string_at(args[2],"aababbaba");exercise();
    }
    guards();
    PRINT("String scenarios/executions/excluded: ");number(cases);PRINT(" ");number(cases*3u);PRINT(" ");number(skipped);
    PRINT("; per service:");for(unsigned i=0;i<7;++i){REQUIRE(paths[i],"service coverage");PRINT(" ");number(paths[i]);}
    PRINT("; stores/caller stores/config stores/changed caller words/cursor updates/empty zero/tokens: ");number(stores);PRINT(" ");number(live_stores);PRINT(" ");number(env_stores);PRINT(" ");number(changed_words);PRINT(" ");number(cursor_updates);PRINT(" ");number(zero_empty);PRINT(" ");number(split_tokens);
    PRINT("; fault/loop/safe probes: ");number(guard_faults);PRINT(" ");number(guard_loops);PRINT(" ");number(guard_safe);
    PRINT("; string extra verified with real strlen/initializer/services 186..192, bytewise memory/environment/caller/EAX/ABI; no helper substitutions\n");
    syscall3(1,0,0,0);
}
