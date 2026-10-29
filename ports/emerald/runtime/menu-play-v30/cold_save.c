/* Sector-only adapter for the already checked PLS1 two-slot transaction.
 * No hardware register, card command or raw-sector entry point is exposed. */
#include "cold_save.h"
#include "cold_copy_v30.h"
#define MAGIC 0x31534350u

static int separate(const void *p,uint32_t n,const void *q,uint32_t m){
 uintptr_t a=(uintptr_t)p,b=(uintptr_t)q;
 return a+n>=a&&b+m>=b&&(a+n<=b||b+m<=a);
}
static void copy(void *p,const void *q,uint32_t n){
 peaq_cold_copy(p,q,n);
}
static int poison(PeaqColdSave *s){s->poisoned=1;return 0;}
static int read_sector(PeaqColdSave *s,uint32_t slot,uint32_t index){
 if(s->poisoned||slot>1||index>=PEAQ_COLD_SAVE_SECTORS)return 0;
 uint32_t lba=s->sectors[slot][index];
 if(s->cached_lba==lba)return 1;
 if(s->reads==0xffffffffu)return poison(s);
 ++s->reads;
 s->cached_lba=0;
 if(s->host.read_sector(s->host.cookie,lba,s->read_buffer)!=1)return poison(s);
 s->cached_lba=lba;
 return 1;
}
static int write_sector(PeaqColdSave *s,uint32_t index){
 if(s->poisoned||!s->writing||s->slot>1||index>=PEAQ_COLD_SAVE_SECTORS)return 0;
 s->cached_lba=0;
 if(s->writes==0xffffffffu)return poison(s);
 ++s->writes;
 if(s->host.write_sector(s->host.cookie,s->sectors[s->slot][index],s->write_buffer)!=1)return poison(s);
 return 1;
}
static int slot_read(void *cookie,uint32_t slot,uint32_t at,void *out,uint32_t want,uint32_t *got){
 PeaqColdSave *s=cookie;
 *got=0;
 if(s->poisoned||s->writing||slot>1||want>4096u)return -1;
 /* Header begins a new complete inspection, never a stale transaction. */
 if(!at)s->cached_lba=0;
 if(at>=PEAQ_COLD_SAVE_RECORD)return 1;
 if(want>PEAQ_COLD_SAVE_RECORD-at)want=PEAQ_COLD_SAVE_RECORD-at;
 uint32_t done=0;
 while(done<want){
  uint32_t offset=at&511u,n=512u-offset;
  if(n>want-done)n=want-done;
  if(!read_sector(s,slot,at/512u))return -1;
  copy((uint8_t *)out+done,s->read_buffer+offset,n);
  done+=n;at+=n;
 }
 *got=done;return 1;
}
static int slot_begin(void *cookie,uint32_t slot){
 PeaqColdSave *s=cookie;
 if(s->poisoned||s->writing||slot>1)return 0;
 s->cached_lba=0;s->writing=1;s->slot=slot;s->position=0;return 1;
}
static int slot_write(void *cookie,const void *data,uint32_t want,uint32_t *wrote){
 PeaqColdSave *s=cookie;*wrote=0;
 if(s->poisoned||!s->writing||!want||want>4096u||
    s->position>PEAQ_COLD_SAVE_RECORD||want>PEAQ_COLD_SAVE_RECORD-s->position)return 0;
 uint32_t done=0;
 while(done<want){
  uint32_t offset=s->position&511u,n=512u-offset;
  if(n>want-done)n=want-done;
  copy(s->write_buffer+offset,(const uint8_t *)data+done,n);
  s->position+=n;done+=n;
  if(!(s->position&511u)&&!write_sector(s,s->position/512u-1u))return 0;
 }
 *wrote=done;return 1;
}
static int slot_finish(void *cookie){
 PeaqColdSave *s=cookie;
 if(!s->writing)return 0;
 if(s->position!=PEAQ_COLD_SAVE_RECORD)poison(s);
 if(!s->poisoned){
  uint32_t n=s->position&511u,index=s->position/512u;
  if(n&&read_sector(s,s->slot,index)){
   copy(s->write_buffer+n,s->read_buffer+n,512u-n);
   write_sector(s,index);
  }
 }
 /* Even a failed/uncertain transfer must reach the adapter's stop/ready gate.
  * Nothing can reuse this context after any I/O failure. */
 if(s->syncs==0xffffffffu)poison(s);
 else{++s->syncs;if(s->host.sync(s->host.cookie)!=1)poison(s);}
 /* Readback must reach the card after acknowledged programming. */
 s->cached_lba=0;s->writing=0;return !s->poisoned;
}
int peaq_cold_save_init(PeaqColdSave *s,const PeaqColdSaveHost *h,
                         const uint32_t map[2][PEAQ_COLD_SAVE_SECTORS]){
 if(!s||!h||!map||((uintptr_t)s&3u)||((uintptr_t)h&3u)||((uintptr_t)map&3u)||
    !separate(s,sizeof(*s),h,sizeof(*h))||!separate(s,sizeof(*s),map,sizeof(s->sectors))||
    !h->read_sector||!h->write_sector||!h->sync||!h->capacity_sectors||
    !h->data_first||h->data_first>=h->capacity_sectors||!h->data_sectors||
    h->data_sectors>h->capacity_sectors-h->data_first)return 0;
 if(s->magic)return 0;
 for(unsigned slot=0;slot<2;++slot)for(unsigned index=0;index<PEAQ_COLD_SAVE_SECTORS;++index){
  uint32_t lba=map[slot][index];
  if(lba<h->data_first||lba-h->data_first>=h->data_sectors)return 0;
  for(unsigned a=0;a<=slot;++a){
   unsigned limit=a==slot?index:PEAQ_COLD_SAVE_SECTORS;
   for(unsigned b=0;b<limit;++b)if(lba==map[a][b])return 0;
  }
 }
 for(unsigned i=0;i<sizeof(*s);++i)((uint8_t *)s)[i]=0;
 copy(&s->host,h,sizeof(*h));copy(s->sectors,map,sizeof(s->sectors));s->magic=MAGIC;
 return 1;
}
static int buffers(PeaqColdSave *s,const void *data,void *scratch){
 return s&&data&&scratch&&!((uintptr_t)s&3u)&&s->magic==MAGIC&&!s->busy&&!s->poisoned&&
  separate(s,sizeof(*s),data,PEAQ_COLD_SAVE_PAYLOAD)&&
  separate(s,sizeof(*s),scratch,PEAQ_SAVE_SCRATCH)&&
  separate(data,PEAQ_COLD_SAVE_PAYLOAD,scratch,PEAQ_SAVE_SCRATCH);
}
static PeaqSaveHost host(PeaqColdSave *s){
 PeaqSaveHost h={s,slot_read,slot_begin,slot_write,slot_finish};return h;
}
static int ready(PeaqColdSave *s){
 return s&&!((uintptr_t)s&3u)&&s->magic==MAGIC&&!s->busy&&!s->poisoned;
}
static int bound_read(void *cookie,uint32_t slot,uint32_t at,void *out,uint32_t n,uint32_t *got){
 PeaqColdSave *s=cookie;
 if(!s||!got||((uintptr_t)got&3u)||!separate(s,sizeof(*s),got,4)||
    !separate(out,n,got,4))return -1;
 *got=0;
 if(!ready(s)||(!out&&n)||n>4096u||!separate(s,sizeof(*s),out,n))return -1;
 s->busy=1;int result=slot_read(s,slot,at,out,n,got);s->busy=0;return result;
}
static int bound_begin(void *cookie,uint32_t slot){
 PeaqColdSave *s=cookie;if(!ready(s))return 0;
 s->busy=1;int result=slot_begin(s,slot);s->busy=0;return result;
}
static int bound_write(void *cookie,const void *data,uint32_t n,uint32_t *wrote){
 PeaqColdSave *s=cookie;
 if(!s||!wrote||((uintptr_t)wrote&3u)||!separate(s,sizeof(*s),wrote,4)||
    !separate(data,n,wrote,4))return 0;
 *wrote=0;
 if(!ready(s)||!data||!separate(s,sizeof(*s),data,n))return 0;
 s->busy=1;int result=slot_write(s,data,n,wrote);s->busy=0;return result;
}
static int bound_finish(void *cookie){
 PeaqColdSave *s=cookie;
 /* finish must still drain an uncertain write after the store was poisoned. */
 if(!s||((uintptr_t)s&3u)||s->magic!=MAGIC||s->busy)return 0;
 s->busy=1;int result=slot_finish(s);s->busy=0;return result;
}
int peaq_cold_save_bind(PeaqColdSave *s,PeaqSaveHost *out){
 if(!out||((uintptr_t)out&3u)||!ready(s)||!separate(s,sizeof(*s),out,sizeof(*out)))return 0;
 PeaqSaveHost h={s,bound_read,bound_begin,bound_write,bound_finish};
 copy(out,&h,sizeof(h));return 1;
}
int peaq_cold_save_load(PeaqColdSave *s,void *data,uint32_t *bytes,void *scratch){
 if(!s||!bytes||((uintptr_t)bytes&3u)||
    !separate(s,sizeof(*s),bytes,4)||!separate(data,PEAQ_COLD_SAVE_PAYLOAD,bytes,4)||
    !separate(scratch,PEAQ_SAVE_SCRATCH,bytes,4))return -1;
 *bytes=0;if(!buffers(s,data,scratch))return -1;
 s->cached_lba=0;s->busy=1;PeaqSaveHost h=host(s);
 int result=peaq_large_save_load(&h,data,PEAQ_COLD_SAVE_PAYLOAD,bytes,scratch,PEAQ_SAVE_SCRATCH);
 if(result==1&&*bytes!=PEAQ_COLD_SAVE_PAYLOAD){*bytes=0;result=-1;}
 s->busy=0;return result;
}
int peaq_cold_save_commit(PeaqColdSave *s,const void *data,void *scratch){
 if(!buffers(s,data,scratch))return 0;
 s->cached_lba=0;s->busy=1;PeaqSaveHost h=host(s);
 int result=peaq_large_save_commit(&h,data,PEAQ_COLD_SAVE_PAYLOAD,scratch,PEAQ_SAVE_SCRATCH);
 s->busy=0;return result;
}
