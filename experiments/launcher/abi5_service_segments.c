#include "abi5_segments.h"
#include <stddef.h>
#define RUN_MAGIC 0x32534750u
typedef struct {
 uint32_t magic,phase,error,generation,started,calling,detached,detaching,lease_sequence,abort;
 uint32_t count,allocated,raw_bytes;
 PeaqResident *owner;const PgaApi *api;void *state,*context;
 PeaqPga5sStart start;PgaEntry event;PeaqPga5sClose close;PeaqPga5sPoll poll;
 void *(*allocate)(void *,uint32_t);void (*deallocate)(void *,void *);
 void (*detach)(void *,uint32_t);void (*release_context)(void *);
 PeaqPga5sMemory segments[PEAQ_PGA5S_MAX_SEGMENTS];
 void *raw[PEAQ_PGA5S_MAX_SEGMENTS];uint32_t raw_sizes[PEAQ_PGA5S_MAX_SEGMENTS];
} Run;
_Static_assert(sizeof(PeaqPga5sDescriptor)==40,"v2 descriptor");
_Static_assert(sizeof(PeaqPga5sSpec)==16,"v2 segment spec");
_Static_assert(sizeof(Run)<=PEAQ_PGA5S_RUN_BYTES,"v2 resident ledger");
static uint32_t addr(const void *p){return (uint32_t)(uintptr_t)p;}
static int range(uint32_t p,uint32_t n){return p&&n&&n<=UINT32_MAX-p;}
static int contains(uint32_t b,uint32_t n,uint32_t p,uint32_t m){return range(b,n)&&p>=b&&p-b<=n&&m<=n-(p-b);}
static int overlap(uint32_t a,uint32_t n,uint32_t b,uint32_t m){return a<b+m&&b<a+n;}
static int thumb(uint32_t b,uint32_t n,uint32_t f){return (f&1u)&&contains(b,n,f&~1u,2);}
static void zero(void *p,uint32_t n){
 uint32_t *words=p;while(n>=4){*words++=0;n-=4;}uint8_t *bytes=(uint8_t *)words;while(n--)*bytes++=0;
}
static int iszero(const void *p,uint32_t n){const uint8_t *b=p;for(uint32_t i=0;i<n;i++)if(b[i])return 0;return 1;}
int peaq_pga5s_validate(const uint32_t *h,const uint8_t *image,PeaqPga5sPlan *out){
 if(!h||!image||!out||h[0]!=0x32414750u)return 0;
 if(h[1]>=2&&h[1]<=4)return -1;
 if(h[1]!=5||h[2]<2||h[2]>65536||h[3]<h[2]||h[3]>131072||h[5]>32768||
    (h[6]&&(h[6]<5||h[6]>1000))||h[7]>512||(h[9]&3u)||h[9]>h[2]||h[2]-h[9]<40||
    (addr(image)&3u)||!range(addr(image),h[2]))return 0;
 PeaqPga5sPlan plan;zero(&plan,sizeof(plan));plan.descriptor=*(const PeaqPga5sDescriptor *)(const void *)(image+h[9]);
 const PeaqPga5sDescriptor *d=&plan.descriptor;uint32_t n=d->segment_count,t=d->table_offset;
 if(d->magic!=PEAQ_PGA5S_MAGIC||d->version!=2||d->bytes!=40||d->flags!=2||!n||n>PEAQ_PGA5S_MAX_SEGMENTS||
    (t&3u)||t>h[2]||h[2]-t<n*16u||overlap(t,n*16u,h[9],40)||h[4]!=d->event_offset)return 0;
 uint32_t offsets[4]={d->start_offset,d->event_offset,d->close_offset,d->poll_offset};
 for(uint32_t i=0;i<4;i++){
  uint32_t o=offsets[i],code=o&~1u;
  if(!(o&1u)||code>=h[2]||h[2]-code<2||overlap(code,2,h[9],40)||overlap(code,2,t,n*16u))return 0;
 }
 for(uint32_t i=0;i<n;i++){
  PeaqPga5sSpec s=((const PeaqPga5sSpec *)(const void *)(image+t))[i];
  if(!s.id||s.flags||!s.bytes||s.bytes>PEAQ_PGA5S_SEGMENT_MAX||s.alignment<4||s.alignment>64||
     (s.alignment&(s.alignment-1u)))return 0;
  for(uint32_t j=0;j<i;j++)if(plan.segments[j].id==s.id)return 0;
  uint32_t raw=s.bytes+s.alignment-1u;
  if(raw>PEAQ_PGA5S_TOTAL_MAX-plan.raw_bytes)return 0;
  plan.raw_bytes+=raw;plan.usable_bytes+=s.bytes;plan.segments[i]=s;
 }
 *out=plan;return 1;
}
static Run *run(PeaqPga5sRun *p){return (Run *)(void *)p;}
static int valid(const Run *r){return r&&r->magic==RUN_MAGIC;}
static void cancelled(void *p){Run *r=p;r->close(r->api,r->state);}
static int pumped(void *p,uint32_t now){Run *r=p;return r->poll(r->api,r->state,now);}
static void drain_segments(Run *r){
 while(r->allocated){
  uint32_t i=--r->allocated;void *p=r->raw[i];r->raw[i]=0;r->segments[i].data=0;
  r->deallocate(r->context,p);
 }
}
static void released(void *p){
 Run *r=p;r->phase=PGS_RELEASING;drain_segments(r);
 void (*release)(void *)=r->release_context;void *context=r->context;
 r->magic=0;release(context); /* Never access any owned allocation afterward. */
}
uint32_t peaq_pga5s_prepare(PeaqPga5sRun *storage,PeaqResident *owner,const PeaqPga5sBinding *input){
 if(!owner||(addr(owner)&3u)||!input)return 0;
 const PeaqPga5sBinding binding=*input;const PeaqPga5sBinding *b=&binding;
 if(!b->header||!b->image||!b->api||!b->context)return 0;
 uint32_t c=addr(b->context),image=addr(b->image),own=addr(owner);
 if((c&3u)||(addr(storage)&3u)||(addr(b->api)&3u)||b->context_bytes>262144||
    !contains(c,b->context_bytes,addr(storage),sizeof(*storage))||
    !contains(c,b->context_bytes,addr(b->api),sizeof(*b->api))||
    overlap(addr(storage),sizeof(*storage),addr(b->api),sizeof(*b->api))||!iszero(storage,sizeof(*storage)))return 0;
 PeaqPga5sPlan plan;if(peaq_pga5s_validate(b->header,b->image,&plan)!=1)return 0;
 uint32_t image_owned=((b->header[3]+7u)&~7u)+b->header[5];
 if((image&31u)||!range(image,image_owned)||!range(own,sizeof(*owner))||
    addr(b->state)!=image+((b->header[3]+7u)&~7u)||b->api->version!=5||b->api->storage!=b->context||
    overlap(image,image_owned,c,b->context_bytes)||overlap(own,sizeof(*owner),c,b->context_bytes)||
    overlap(own,sizeof(*owner),image,image_owned))return 0;
 uint32_t callbacks[4]={addr((const void *)b->allocate),addr((const void *)b->deallocate),
   addr((const void *)b->detach),addr((const void *)b->release_context)};
 for(uint32_t i=0;i<4;i++)if(!thumb(b->resident_base,b->resident_bytes,callbacks[i]))return 0;
 if(overlap(b->resident_base,b->resident_bytes,image,image_owned)||
    overlap(b->resident_base,b->resident_bytes,c,b->context_bytes)||overlap(b->resident_base,b->resident_bytes,own,sizeof(*owner)))return 0;
 Run *r=run(storage);const PeaqPga5sDescriptor *d=&plan.descriptor;
 *r=(Run){.magic=RUN_MAGIC,.phase=PGS_PREPARING,.count=d->segment_count,.raw_bytes=plan.raw_bytes,
  .owner=owner,.api=b->api,.state=b->state,.context=b->context,
  .start=(PeaqPga5sStart)(uintptr_t)(image+d->start_offset),.event=(PgaEntry)(uintptr_t)(image+d->event_offset),
  .close=(PeaqPga5sClose)(uintptr_t)(image+d->close_offset),.poll=(PeaqPga5sPoll)(uintptr_t)(image+d->poll_offset),
  .allocate=b->allocate,.deallocate=b->deallocate,.detach=b->detach,.release_context=b->release_context};
 PeaqResidentStatus status;peaq_resident_status(owner,&status);
 if(status.state||status.busy){r->error=PGS_BUSY;goto failure;}
 for(uint32_t i=0;i<r->count;i++){
  const PeaqPga5sSpec *s=&plan.segments[i];uint32_t raw_size=s->bytes+s->alignment-1u;
  void *allocation=r->allocate(r->context,raw_size);uint32_t raw=addr(allocation);
  if(!allocation){r->error=PGS_OOM;goto failure;}
  /* Reject broken allocator aliases without freeing an unowned/duplicate
   * pointer. Only successfully adopted fresh allocations enter this ledger. */
  if(!range(raw,raw_size)||overlap(raw,raw_size,c,b->context_bytes)||overlap(raw,raw_size,image,image_owned)||
     overlap(raw,raw_size,own,sizeof(*owner))||overlap(raw,raw_size,b->resident_base,b->resident_bytes)){
   r->error=PGS_ALLOCATOR;goto failure;
  }
  for(uint32_t j=0;j<i;j++)if(overlap(raw,raw_size,addr(r->raw[j]),r->raw_sizes[j])){r->error=PGS_ALLOCATOR;goto failure;}
  uint32_t data=(raw+s->alignment-1u)&~(s->alignment-1u);
  r->raw[i]=allocation;r->raw_sizes[i]=raw_size;r->allocated=i+1;
  r->segments[i]=(PeaqPga5sMemory){s->id,(void *)(uintptr_t)data,s->bytes,s->alignment};
  if(r->abort){r->error=PGS_ABORTED;goto failure;}
  zero((void *)(uintptr_t)data,s->bytes);
 }
 if(r->abort){r->error=PGS_ABORTED;goto failure;}
 PeaqResidentOps ops={r,cancelled,pumped,released};
 uint32_t generation=peaq_resident_claim(owner,&ops);
 if(!generation){r->error=PGS_CLAIM;goto failure;}
 r->generation=generation;r->phase=PGS_READY;return generation;
failure:
 drain_segments(r);r->phase=PGS_FAILED;return 0;
}
static int lease(Run *r,const PeaqResidentUse *use){
 if(!valid(r)||r->phase!=PGS_READY||!use||r->calling||use->cookie!=r||use->generation!=r->generation||
    !r->lease_sequence||use->sequence!=r->lease_sequence)return 0;
 PeaqResidentStatus s;peaq_resident_status(r->owner,&s);
 return s.generation==r->generation&&s.busy==1&&s.lease_sequence==r->lease_sequence;
}
int peaq_pga5s_enter(PeaqPga5sRun *storage,PeaqResidentUse *use){
 Run *r=run(storage);if(!valid(r)||r->phase!=PGS_READY||r->detached||!peaq_resident_enter(r->owner,r->generation,use))return 0;
 r->lease_sequence=use->sequence;return 1;
}
int peaq_pga5s_close(PeaqPga5sRun *storage){
 Run *r=run(storage);if(!valid(r)||(r->phase!=PGS_PREPARING&&r->phase!=PGS_READY))return 0;
 if(r->phase==PGS_PREPARING)r->abort=1;
 if(r->detaching)return 1;
 if(!r->detached){r->detached=1;r->detaching=1;r->detach(r->context,r->generation);r->detaching=0;}
 if(r->phase==PGS_PREPARING)return 1;
 PeaqResident *owner=r->owner;uint32_t generation=r->generation;return peaq_resident_close(owner,generation);
}
int peaq_pga5s_start(PeaqPga5sRun *storage,const PeaqResidentUse *use){
 Run *r=run(storage);if(!lease(r,use)||r->started||r->detached)return 0;
 r->started=1;r->calling=1;int result=r->start(r->api,r->state,r->segments,r->count,r->generation);r->calling=0;
 if(result!=1||r->detached){peaq_pga5s_close(storage);return 0;}return 1;
}
uint32_t peaq_pga5s_event(PeaqPga5sRun *storage,const PeaqResidentUse *use,uint32_t event,uint32_t value){
 Run *r=run(storage);if(!lease(r,use)||!r->started||r->detached||event<PGA_PAINT||event>PGA_RAW_KEY||event==PGA_CLOSE)return 0;
 r->calling=1;uint32_t result=r->event(r->api,r->state,event,value);r->calling=0;
 if(result&PGA_QUIT)peaq_pga5s_close(storage);return result;
}
int peaq_pga5s_leave(PeaqPga5sRun *storage,const PeaqResidentUse *use){
 Run *r=run(storage);if(!lease(r,use))return 0;PeaqResident *owner=r->owner;
 r->lease_sequence=0;return peaq_resident_leave(owner,use);
}
void peaq_pga5s_status(const PeaqPga5sRun *storage,PeaqPga5sStatus *out){
 const Run *r=(const Run *)(const void *)storage;if(!out)return;*out=(PeaqPga5sStatus){0};
 if(valid(r))*out=(PeaqPga5sStatus){r->phase,r->error,r->generation,r->count,r->allocated,r->raw_bytes,r->detached};
}
static const char SOURCE[]="PGA5 segmented runtime v2";
void *peaq_pga5s_native_allocate(void *context,uint32_t bytes){
 (void)context;return ((void *(*)(uint32_t,const char *,uint32_t))0x88090848u)(bytes,SOURCE,1);
}
void peaq_pga5s_native_deallocate(void *context,void *allocation){
 (void)context;((void (*)(void *,const char *,uint32_t))0x88090850u)(allocation,SOURCE,1);
}
