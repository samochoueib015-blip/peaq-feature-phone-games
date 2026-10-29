#include "abi5.h"
#include <stddef.h>
#define RUN_MAGIC 0x354e5552u
typedef struct {
 uint32_t magic,generation,started,calling,detached,lease_sequence,detaching;
 PeaqResident *owner;const PgaApi *api;void *state,*arena,*context;
 uint32_t arena_bytes;
 PeaqPga5Start start;PgaEntry event;PeaqPga5Close close;PeaqPga5Poll poll;
 void (*detach)(void *,uint32_t);void (*release)(void *);
} Run;
_Static_assert(sizeof(PeaqPga5Descriptor)==40,"PGA5 descriptor size");
_Static_assert(sizeof(Run)<=PEAQ_PGA5_RUN_BYTES,"PGA5 host run storage");
static uint32_t address(const void *p){return (uint32_t)(uintptr_t)p;}
static int range(uint32_t base,uint32_t bytes){return base&&bytes&&bytes<=UINT32_MAX-base;}
static int contains(uint32_t base,uint32_t bytes,uint32_t p,uint32_t n){
 return range(base,bytes)&&p>=base&&p-base<=bytes&&n<=bytes-(p-base);
}
static int overlap(uint32_t a,uint32_t n,uint32_t b,uint32_t m){return a<b+m&&b<a+n;}
static int thumb(uint32_t base,uint32_t bytes,uint32_t target){
 return (target&1u)&&contains(base,bytes,target&~1u,2);
}
int peaq_pga5_validate(const uint32_t *h,const uint8_t *image,PeaqPga5Descriptor *out){
 if(!h||!image||!out||h[0]!=0x32414750u)return 0;
 if(h[1]>=2&&h[1]<=4)return -1;
 if(h[1]!=5||h[2]<2||h[2]>65536||h[3]<h[2]||h[3]>131072||h[5]>32768||
    (h[6]&&(h[6]<50||h[6]>1000))||h[7]>512||h[9]&3u||h[9]>h[2]||h[2]-h[9]<40)return 0;
 if((address(image)&3u)||!range(address(image),h[2]))return 0;
 PeaqPga5Descriptor d=*(const PeaqPga5Descriptor *)(const void *)(image+h[9]);
 if(d.magic!=PEAQ_PGA5_MAGIC||d.version!=1||d.bytes!=40||d.flags!=1||d.arena_alignment!=32||
    !d.arena_bytes||d.arena_bytes>PEAQ_PGA5_ARENA_MAX||h[4]!=d.event_offset)return 0;
 uint32_t offsets[4]={d.start_offset,d.event_offset,d.close_offset,d.poll_offset};
 for(uint32_t i=0;i<4;i++){
  uint32_t o=offsets[i],code=o&~1u;
  if(!(o&1u)||code>=h[2]||h[2]-code<2||(code>=h[9]&&code<h[9]+40))return 0;
 }
 *out=d;return 1;
}
static Run *run(PeaqPga5Run *r){return (Run *)(void *)r;}
static int valid(const Run *r){return r&&r->magic==RUN_MAGIC;}
static int zero(const PeaqPga5Run *r){
 if(!r)return 0;for(uint32_t i=0;i<sizeof(*r);i++)if(r->bytes[i])return 0;return 1;
}
static void cancelled(void *p){Run *r=p;r->close(r->api,r->state);}
static int pumped(void *p,uint32_t now){Run *r=p;return r->poll(r->api,r->state,now);}
static void released(void *p){
 Run *r=p;void (*release)(void *)=r->release;void *context=r->context;
 r->magic=0;release(context); /* No owned read/write after this point. */
}
uint32_t peaq_pga5_bind(PeaqPga5Run *storage,PeaqResident *owner,const PeaqPga5Binding *b){
 if(!owner||(address(owner)&3u)||!b||!b->header||!b->image||!b->api||!b->arena||!b->context)return 0;
 uint32_t context=address(b->context),image=address(b->image),arena=address(b->arena);
 if(context&3u||(address(b->api)&3u)||(address(storage)&3u)||b->context_bytes>262144||!contains(context,b->context_bytes,address(storage),sizeof(*storage))||
    !contains(context,b->context_bytes,address(b->api),sizeof(*b->api))||
    overlap(address(storage),sizeof(*storage),address(b->api),sizeof(*b->api))||!zero(storage))return 0;
 PeaqPga5Descriptor d;if(peaq_pga5_validate(b->header,b->image,&d)!=1)return 0;
 uint32_t image_owned=((b->header[3]+7u)&~7u)+b->header[5];
 if(image&31u||arena&31u||b->arena_bytes!=d.arena_bytes||!range(image,image_owned)||!range(arena,b->arena_bytes)||
    address(b->state)!=image+((b->header[3]+7u)&~7u)||b->api->storage!=b->context||b->api->version!=5||
    overlap(image,image_owned,arena,b->arena_bytes)||overlap(image,image_owned,context,b->context_bytes)||
    overlap(arena,b->arena_bytes,context,b->context_bytes)||!range(address(owner),sizeof(*owner))||
    overlap(address(owner),sizeof(*owner),image,image_owned)||
    overlap(address(owner),sizeof(*owner),arena,b->arena_bytes)||
    overlap(address(owner),sizeof(*owner),context,b->context_bytes)||
    !thumb(b->resident_base,b->resident_bytes,address((const void *)b->detach))||
    !thumb(b->resident_base,b->resident_bytes,address((const void *)b->release))||
    overlap(b->resident_base,b->resident_bytes,image,image_owned)||
    overlap(b->resident_base,b->resident_bytes,arena,b->arena_bytes)||
    overlap(b->resident_base,b->resident_bytes,context,b->context_bytes))return 0;
 Run *r=run(storage);
 *r=(Run){.magic=RUN_MAGIC,.owner=owner,.api=b->api,.state=b->state,.arena=b->arena,.context=b->context,
   .arena_bytes=d.arena_bytes,.start=(PeaqPga5Start)(uintptr_t)(image+d.start_offset),
   .event=(PgaEntry)(uintptr_t)(image+d.event_offset),.close=(PeaqPga5Close)(uintptr_t)(image+d.close_offset),
   .poll=(PeaqPga5Poll)(uintptr_t)(image+d.poll_offset),.detach=b->detach,.release=b->release};
 PeaqResidentOps ops={r,cancelled,pumped,released};
 uint32_t generation=peaq_resident_claim(owner,&ops);
 if(!generation){for(uint32_t i=0;i<sizeof(*storage);i++)storage->bytes[i]=0;return 0;}
 r->generation=generation;return generation;
}
static int lease(Run *r,const PeaqResidentUse *use){
 if(!valid(r)||!use||r->calling||use->cookie!=r||use->generation!=r->generation||
    use->sequence!=r->lease_sequence||!r->lease_sequence)return 0;
 PeaqResidentStatus s;peaq_resident_status(r->owner,&s);
 return s.generation==r->generation&&s.busy==1&&s.lease_sequence==r->lease_sequence;
}
int peaq_pga5_enter(PeaqPga5Run *storage,PeaqResidentUse *use){
 Run *r=run(storage);if(!valid(r)||r->detached||!peaq_resident_enter(r->owner,r->generation,use))return 0;
 r->lease_sequence=use->sequence;return 1;
}
int peaq_pga5_close(PeaqPga5Run *storage){
 Run *r=run(storage);if(!valid(r))return 0;
 if(r->detaching)return 1;
 if(!r->detached){
  /* Reentrant close during native detach must NOT transition the owner to
   * CLOSING: a nested poll could otherwise free this still-active stack's
   * Context. Mark the view inaccessible first, then perform detach once. */
  r->detached=1;r->detaching=1;r->detach(r->context,r->generation);r->detaching=0;
 }
 PeaqResident *owner=r->owner;uint32_t generation=r->generation;
 return peaq_resident_close(owner,generation);
}
int peaq_pga5_start(PeaqPga5Run *storage,const PeaqResidentUse *use){
 Run *r=run(storage);if(!lease(r,use)||r->started||r->detached)return 0;
 r->started=1;r->calling=1;int result=r->start(r->api,r->state,r->arena,r->arena_bytes);r->calling=0;
 if(result!=1||r->detached){peaq_pga5_close(storage);return 0;}return 1;
}
uint32_t peaq_pga5_event(PeaqPga5Run *storage,const PeaqResidentUse *use,uint32_t event,uint32_t value){
 Run *r=run(storage);if(!lease(r,use)||!r->started||r->detached||event<PGA_PAINT||event>PGA_RAW_KEY||event==PGA_CLOSE)return 0;
 r->calling=1;uint32_t result=r->event(r->api,r->state,event,value);r->calling=0;
 if(result&PGA_QUIT)peaq_pga5_close(storage);return result;
}
int peaq_pga5_leave(PeaqPga5Run *storage,const PeaqResidentUse *use){
 Run *r=run(storage);if(!lease(r,use))return 0;
 PeaqResident *owner=r->owner;r->lease_sequence=0;return peaq_resident_leave(owner,use);
}
