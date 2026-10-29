#include "large_save.h"
#define MAGIC 0x31534c50u /* PLS1, little endian */
typedef struct {uint32_t words[5];int present,valid;} Record;
static uint32_t hash(uint32_t h,const void *p,uint32_t n){
 const uint8_t *b=p;while(n--)h=(h^*b++)*16777619u;return h;
}
static int buffers(const void *data,uint32_t n,const void *scratch,uint32_t sn){
 uintptr_t d=(uintptr_t)data,s=(uintptr_t)scratch;
 return data&&scratch&&n&&n<=PEAQ_SAVE_MAX&&sn&&sn<=PEAQ_SAVE_SCRATCH&&
        d+n>=d&&s+sn>=s&&(d+n<=s||s+sn<=d);
}
/* -1: I/O, 0: truncated, 1: exact, 2: absent. */
static int read_exact(const PeaqSaveHost *h,uint32_t slot,uint32_t at,void *p,uint32_t n){
 uint8_t *out=p;uint32_t done=0;
 while(done<n){
  uint32_t got=0,want=n-done;if(want>4096)want=4096;
  int r=h->read(h->cookie,slot,at+done,out+done,want,&got);
  if(r==0)return at+done==0?2:-1;
  if(r!=1||got>want)return -1;
  if(!got)return 0;done+=got;
 }return 1;
}
static int inspect(const PeaqSaveHost *h,uint32_t slot,Record *r,uint8_t *scratch,uint32_t sn){
 r->present=r->valid=0;
 int result=read_exact(h,slot,0,r->words,20);
 if(result<0)return 0;if(result==2)return 1;r->present=1;
 if(result!=1||r->words[0]!=MAGIC||r->words[1]!=1||!r->words[2]||r->words[2]>PEAQ_SAVE_MAX)return 1;
 uint32_t digest=hash(2166136261u,r->words,16),n=r->words[2];
 for(uint32_t at=0;at<n;){
  uint32_t want=n-at;if(want>sn)want=sn;
  result=read_exact(h,slot,20+at,scratch,want);
  if(result<0)return 0;if(result!=1)return 1;
  digest=hash(digest,scratch,want);at+=want;
 }
 uint32_t extra=0;int tail=h->read(h->cookie,slot,20+n,scratch,1,&extra);
 if(tail!=1||extra>1)return 0;
 r->valid=!extra&&digest==r->words[4];return 1;
}
/* -1 = none, -2 = sequence ambiguity, otherwise newest slot. */
static int newest(const Record r[2]){
 if(!r[0].valid)return r[1].valid?1:-1;if(!r[1].valid)return 0;
 uint32_t diff=r[0].words[3]-r[1].words[3];
 if(!diff)return r[0].words[2]==r[1].words[2]&&r[0].words[4]==r[1].words[4]?0:-2;
 if(diff==0x80000000u)return -2;return diff<0x80000000u?0:1;
}
int peaq_large_save_load(const PeaqSaveHost *h,void *data,uint32_t cap,uint32_t *bytes,void *scratch,uint32_t sn){
 if(!bytes)return -1;*bytes=0;
 if(!h||!h->read||!buffers(data,cap,scratch,sn))return -1;
 Record r[2];
 if(!inspect(h,0,r,scratch,sn)||!inspect(h,1,r+1,scratch,sn))return -1;
 int slot=newest(r);if(slot==-2)return -1;if(slot==-1)return 0;
 uint32_t n=r[slot].words[2];if(n>cap)return -1;
 if(read_exact(h,(uint32_t)slot,20,data,n)!=1||hash(hash(2166136261u,r[slot].words,16),data,n)!=r[slot].words[4])return -1;
 *bytes=n;return 1;
}
int peaq_large_save_commit(const PeaqSaveHost *h,const void *data,uint32_t n,void *scratch,uint32_t sn){
 if(!h||!h->read||!h->begin||!h->write||!h->finish||!buffers(data,n,scratch,sn))return 0;
 Record r[2];if(!inspect(h,0,r,scratch,sn)||!inspect(h,1,r+1,scratch,sn))return 0;
 int old=newest(r);if(old==-2||(old==-1&&(r[0].present||r[1].present)))return 0;
 uint32_t slot=old<0?0:1u-(uint32_t)old;
 uint32_t v[5]={MAGIC,1,n,old<0?0:r[old].words[3]+1,0};v[4]=hash(hash(2166136261u,v,16),data,n);
 if(h->begin(h->cookie,slot)!=1)return 0;
 uint32_t wrote=0;int ok=h->write(h->cookie,v,20,&wrote)==1&&wrote==20;
 for(uint32_t at=0;ok&&at<n;){
  uint32_t want=n-at;if(want>4096)want=4096;wrote=0;
  ok=h->write(h->cookie,(const uint8_t *)data+at,want,&wrote)==1&&wrote==want;at+=want;
 }
 if(h->finish(h->cookie)!=1)ok=0;if(!ok)return 0;
 if(!inspect(h,slot,r+slot,scratch,sn)||!r[slot].valid)return 0;
 for(uint32_t i=0;i<5;i++)if(r[slot].words[i]!=v[i])return 0;
 return 1;
}
