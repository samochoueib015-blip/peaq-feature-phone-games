#include "module.h"
static uint32_t hash(uint32_t h,const void *p,uint32_t n) {
    const uint8_t *b=p;
    while(n&&((uintptr_t)b&3u)){h=(h^*b++)*16777619u;--n;}
    while(n>=4){
        uint32_t v=*(const uint32_t *)b;b+=4;n-=4;
        h=(h^(v&255u))*16777619u;
        h=(h^((v>>8)&255u))*16777619u;
        h=(h^((v>>16)&255u))*16777619u;
        h=(h^(v>>24))*16777619u;
    }
    while(n--)h=(h^*b++)*16777619u;return h;
}
/* Aligned words are safe. For packed PGE2 relocations force byte accesses;
 * never depend on the CPU allowing unaligned word transfers. */
static uint32_t get_word(const volatile uint8_t *p) {
    if(!((uintptr_t)p&3u))return *(const volatile uint32_t *)p;
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void put_word(volatile uint8_t *p,uint32_t v) {
    if(!((uintptr_t)p&3u)){*(volatile uint32_t *)p=v;return;}
    p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);
}
static void zero(void *p,uint32_t n){
 uint8_t *b=p;
 while(n&&((uintptr_t)b&3u)){*b++=0;--n;}
 while(n>=4){*(uint32_t *)b=0;b+=4;n-=4;}
 while(n--)*b++=0;
}
static int exact(const PeaqModuleHost *h,void *p,uint32_t n) {
    uint8_t *dst=p;
    while(n) {
        uint32_t got=0,request=n>4096?4096:n;
        if(!h->read(h->cookie,dst,request,&got) || !got || got>request)return 0;
        dst+=got;n-=got;
    }
    return 1;
}
int peaq_module_load(const PeaqModuleHost *h,PeaqModule *out) {
    uint32_t v[8],rel[256],digest,base,end,previous=0,seen=0;
    if(!out)return -1;
    *out=(PeaqModule){0};
    if(!h || !h->read || !h->allocate || !h->release || !h->sync_code)return -1;
    if(!exact(h,v,sizeof(v)))return -2;
    if(v[0]!=0x31454750 || (v[1]!=1 && v[1]!=2) || v[2]<4 || v[2]>v[3] || v[3]>24u*1024u*1024u ||
       (v[4]&3) || v[5]<1 || v[5]>32 || v[4]>v[2] || v[5]*4>v[2]-v[4] || v[6]>100000)return -3;
    out->allocation=h->allocate(v[3]+31);
    if(!out->allocation)return -4;
    base=((uint32_t)(uintptr_t)out->allocation+31)&~31u;end=base+v[3];
    if(base<(uint32_t)(uintptr_t)out->allocation || end<base)goto invalid;
    out->base=(uint8_t *)(uintptr_t)base;
    if(!exact(h,out->base,v[2]))goto invalid;
    digest=hash(hash(2166136261u,v,28),out->base,v[2]);
    zero(out->base+v[2],v[3]-v[2]);
    for(uint32_t i=0;i<v[6];) {
        uint32_t n=v[6]-i;if(n>256)n=256;
        if(!exact(h,rel,n*4))goto invalid;
        digest=hash(digest,rel,n*4);
        for(uint32_t j=0;j<n;j++) {
            uint32_t at=rel[j];
            if((v[1]==1 && (at&3)) || at>v[2]-4 || ((i||j) && at<previous+4))goto invalid;
            previous=at;
            uint8_t *word=out->base+at;
            uint32_t value=get_word(word);
            if(value>=v[3])goto invalid;
            if(at<v[4]+v[5]*4 && at+4>v[4]) {
                if(at<v[4] || ((at-v[4])&3))goto invalid;
                seen++;
            }
            put_word(word,value+base);
        }
        i+=n;
    }
    if(digest!=v[7] || seen!=v[5])goto invalid;
    uint8_t extra;uint32_t got=0;
    if(!h->read(h->cookie,&extra,1,&got) || got)goto invalid;
    out->exports=(uint32_t *)(out->base+v[4]);
    for(uint32_t i=0;i<v[5];i++) {
        uint32_t p=out->exports[i];
        if(!(p&1) || (p&~1u)<base || (p&~1u)-base>=v[2])goto invalid;
    }
    out->image_bytes=v[2];out->memory_bytes=v[3];out->export_count=v[5];
    h->sync_code(out->base,v[2]);
    return 1;
invalid:
    h->release(out->allocation);*out=(PeaqModule){0};return -5;
}
