/* The remaining string APIs at 0x82a531..0x82a9bb. These preserve the
 * original unchecked memory, caller-word updates and nonstandard results. */
#include "string_extra.h"
#include "helpers.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))

uint32_t kernel_string_upper_82a531(volatile uint32_t text)
{
    uint32_t i=0;
    for(;BYTE(text+i);++i)
        if((int8_t)BYTE(text+i)>96 && (int8_t)BYTE(text+i)<=122)
            BYTE(text+i)=(uint8_t)(BYTE(text+i)-32u);
    return text+i;
}

uint32_t kernel_string_find_82a57d(uint32_t text,uint32_t needle)
{
    if(!BYTE(needle))return text;
    for(uint32_t start=text;BYTE(start);++start){
        if(BYTE(start)!=BYTE(needle))continue;
        uint32_t a=start,b=needle;
        while(BYTE(a)&&BYTE(b)&&BYTE(a)==BYTE(b)){++a;++b;}
        if(!BYTE(b))return start;
    }
    return 0;
}

uint32_t kernel_config_value_82a609(volatile uint32_t output,uint32_t name)
{
    if(!BYTE(name)||BYTE(name)==' ')return 0;
    for(uint32_t start=0x62200u;BYTE(start);++start){
        if(BYTE(start)!=BYTE(name))continue;
        uint32_t a=start,b=name;
        while(BYTE(a)&&BYTE(b)&&BYTE(a)==BYTE(b)){++a;++b;}
        if(BYTE(b)||BYTE(a)!='=')continue;
        ++a;
        uint32_t i=0;
        for(;i<255u;++i){
            if(BYTE(a+i)==';'){BYTE(output+i)=0;return 1;}
            BYTE(output+i)=BYTE(a+i);
        }
        BYTE(output+i)=0;
        return 1;
    }
    return 0;
}

uint32_t kernel_string_character_82a70d(volatile uint32_t text,uint32_t character)
{
    /* Incrementing the incoming stack word is observable by the caller. */
    while(BYTE(text)&&BYTE(text)!=(uint8_t)character)++text;
    return BYTE(text)==(uint8_t)character?text:0;
}

uint32_t kernel_character_upper_82a755(uint32_t character)
{
    int8_t value=(int8_t)character;
    if(value>96 && value<=122)value=(int8_t)(value-32);
    return (uint32_t)(int32_t)value;
}

uint32_t kernel_unsigned_text_82a787(uint32_t value,volatile uint32_t output,
    volatile uint32_t radix,volatile uint32_t minimum)
{
    uint32_t temporary=value,digits=0;
    if(temporary){
        while(temporary){temporary/=radix;++digits;}
        if((int32_t)digits<(int32_t)minimum)digits=minimum;
        temporary=value;
        BYTE(output+digits)=0;
        for(--digits;(int32_t)digits>=0;--digits){
            BYTE(output+digits)=(uint8_t)(temporary%radix+48u);
            if((int8_t)BYTE(output+digits)>57)
                BYTE(output+digits)=(uint8_t)(BYTE(output+digits)+7u);
            temporary/=radix;
        }
    }else{
        BYTE(output)='0';BYTE(output+1u)=0;
        for(;(int32_t)digits<(int32_t)minimum;++digits)BYTE(output+digits)='0';
        BYTE(output+digits)=0;
    }
    return output;
}

uint32_t kernel_string_split_82a8a3(volatile uint32_t tokens,
    volatile uint32_t delimiter,volatile uint32_t text)
{
    uint32_t length=kernel_string_length(text),separator=kernel_string_length(delimiter),count=0;
    for(uint32_t i=0;(int32_t)i<(int32_t)length;++i){
        uint32_t j=0,same=1;
        for(;(int32_t)j<(int32_t)separator;++j)
            if(BYTE(delimiter+j)!=BYTE(text+i+j)){same=0;break;}
        if(!same)continue;
        for(j=0;(int32_t)j<(int32_t)separator;++j)BYTE(text+i+j)=0;
        i+=separator-1u;
    }
    uint32_t inside=0;
    for(uint32_t i=0;(int32_t)i<(int32_t)length;++i){
        if(!BYTE(text+i))inside=0;
        else if(!inside){inside=1;DWORD(tokens+count*4u)=text+i;++count;}
    }
    return count;
}
