/* ARMv7 short descriptors, TRE=AFE=EAE=0, domain 0 client, privileged only.
 * Formats checked against ARM CMSIS Core_A descriptor definitions. This is
 * an independent fixed map, not a copy of inherited loader page tables. */
#include "data_cache.h"

uint32_t peaq_dc_tables[PEAQ_DC_TABLE_BYTES/4]
 __attribute__((section(".cache_tables"),aligned(16384)));

typedef struct {uint32_t begin,end,texcb,xn,ro;} Region;
/* TEX[2]=1: normal memory. TEX[1:0] and C/B select outer/inner policy.
 * 0x10 = outer/inner noncacheable; 0x15 = outer/inner WB write allocate.
 * 0x01 = shared Device. Descriptor S=1 throughout. */
static unsigned regions(Region *r,uint32_t text_end){
 if(text_end<0x80101000u||text_end>0x8010f000u||(text_end&4095u))return 0;
 r[0]=(Region){0x20800000u,0x20801000u,1,1,0}; /* LCDC */
 r[1]=(Region){0x20a00000u,0x20a01000u,1,1,0}; /* LCM bus */
 r[2]=(Region){0x20e01000u,0x20e02000u,1,1,0}; /* LCD error reset */
 r[3]=(Region){0x40230000u,0x40231000u,1,1,1}; /* timer read only */
 r[4]=(Region){0x40250000u,0x40251000u,1,1,0}; /* keypad polling */
 r[5]=(Region){0x40280000u,0x40281000u,1,1,0}; /* panel GPIO */
 r[6]=(Region){0x60000000u,0x60001000u,1,1,0}; /* panel command */
 r[7]=(Region){0x800f0000u,0x80100000u,0x10,1,0}; /* main stack */
 r[8]=(Region){0x80100000u,text_end,0x15,0,1}; /* code/constant data */
 r[9]=(Region){text_end,0x80110000u,0x10,1,0}; /* report/exception stacks */
 r[10]=(Region){0x80400000u,PEAQ_DC_GAME_BEGIN,0x10,1,0}; /* FAT */
 r[11]=(Region){PEAQ_DC_GAME_BEGIN,0x810f0000u,0x15,0,0};
 r[12]=(Region){PEAQ_DC_SCRATCH_BASE,PEAQ_DC_SCRATCH_BASE+PEAQ_DC_SCRATCH_BYTES,0x15,1,0};
 r[13]=(Region){PEAQ_DC_TABLE_BASE,PEAQ_DC_TABLE_BASE+PEAQ_DC_TABLE_BYTES,0x10,1,0};
 r[14]=(Region){0x813d8000u,0x813fe000u,0x10,1,0}; /* LCD DMA + guards */
 return 15;
}
static uint32_t attributes(Region r,int section){
 uint32_t ap=1u|(r.ro?4u:0u); /* privileged RW or RO, user no access */
 if(section)return 2u|(r.xn<<4)|(ap&3u)<<10|(ap>>2)<<15|
  (1u<<16)|(r.texcb&3u)<<2|(r.texcb>>2)<<12;
 return 2u|r.xn|(ap&3u)<<4|(ap>>2)<<9|(1u<<10)|
  (r.texcb&3u)<<2|(r.texcb>>2)<<6;
}
int peaq_dc_build_tables(uint32_t text_end){
 Region r[15];unsigned n=regions(r,text_end),used=0;
 if(!n||(uintptr_t)peaq_dc_tables!=PEAQ_DC_TABLE_BASE)return 0;
 for(unsigned i=0;i<PEAQ_DC_TABLE_BYTES/4;++i)peaq_dc_tables[i]=0;
 for(unsigned i=0;i<n;++i){
  for(uint32_t a=r[i].begin;a<r[i].end;){
   uint32_t *first=peaq_dc_tables+(a>>20);
   if(!(a&0xfffffu)&&r[i].end-a>=0x100000u){
    if(*first)return 0;
    *first=a|attributes(r[i],1);a+=0x100000u;
   }else{
    if(!*first){
     if(used>=16)return 0;
     *first=(PEAQ_DC_TABLE_BASE+0x4000u+used++*0x400u)|1u;
    }
    if((*first&3u)!=1u)return 0;
    uint32_t *second=(uint32_t*)(uintptr_t)(*first&~0x3ffu);
    unsigned index=(a>>12)&255u;
    if(second[index])return 0;
    second[index]=a|attributes(r[i],0);a+=4096u;
   }
  }
 }
 peaq_dc.tables_used=used;
 return peaq_dc_check_tables(text_end);
}
/* Validate the entire 4 GiB virtual address space at 4 KiB granularity,
 * including every absent mapping. No generic trusted range parameters. */
int peaq_dc_check_tables(uint32_t text_end){
 Region r[15];unsigned n=regions(r,text_end),slot=0;
 uint32_t hash=0x811c9dc5u;
 if(!n||(uintptr_t)peaq_dc_tables!=PEAQ_DC_TABLE_BASE)return 0;
 for(unsigned i=0;i<4096;++i){
  uint32_t first=peaq_dc_tables[i];
  if(!first){
   for(unsigned k=0;k<n;++k)
    if((r[k].begin>>20)<=i&&((r[k].end-1u)>>20)>=i)return 0;
   continue;
  }
  if((first&3u)==1u){
   if(first!=(PEAQ_DC_TABLE_BASE+0x4000u+slot*0x400u+1u)||slot>=16)return 0;
   ++slot;
  }else if(first&&(first&3u)!=2u)return 0;
  for(unsigned j=0;j<256;++j){
   uint32_t a=(i<<20)|(j<<12),want=0,got=0;
   for(unsigned k=0;k<n;++k)if(a>=r[k].begin&&a<r[k].end){
    want=((first&3u)==2u?(a&0xfff00000u):a)|attributes(r[k],(first&3u)==2u);
    break;
   }
   if((first&3u)==2u)got=first;
   else if(first)got=((uint32_t*)(uintptr_t)(first&~0x3ffu))[j];
   if(got!=want)return 0;
  }
 }
 if(slot!=peaq_dc.tables_used)return 0;
 for(unsigned i=4096u+slot*256u;i<PEAQ_DC_TABLE_BYTES/4;++i)
  if(peaq_dc_tables[i])return 0;
 for(unsigned i=0;i<PEAQ_DC_TABLE_BYTES/4;++i)hash=(hash^peaq_dc_tables[i])*0x01000193u;
 peaq_dc.table_checksum=hash;return 1;
}
