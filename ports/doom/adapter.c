#include "native_controls.h"
#include "pga_service_adapter.h"
#include "../launcher/segment_profiles.h"
#include "../runtime/mocor_display.h"
#include "../runtime/mocor_time.h"
#include "platform.h"
#include <stddef.h>

#define MAGIC 0x31504444u
const u32 pga_state_bytes=sizeof(PeaqDoomPgaState),pga_tick_ms=5,pga_required_abi=5;
/* The packer fills numeric OFFSETS here after linking. These words must never
 * be relocation targets, as required by the experimental resident validator. */
const uint32_t peaq_doom_pga_descriptor[10]={0};
const PeaqPga5sSpec peaq_doom_pga_segments[7]={
 {PGS_PORT_MODULE,PEAQ_DOOM_MODULE_BYTES,4,0},
 {PGS_PORT_HEAP,PEAQ_DOOM_HEAP_BYTES,4,0},
 {PGS_PORT_FRAMES,PEAQ_DOOM_FRAME_BYTES,4,0},
 {PGS_PORT_WORKER,PEAQ_DOOM_WORKER_BYTES,4,0},
 {PGS_PORT_HOST,PEAQ_DOOM_MOCOR_HOST_BYTES,4,0},
 {PGS_PORT_STACK,PEAQ_DOOM_STACK_BYTES,8,0},
 {PGS_PORT_TCB,PEAQ_DOOM_TCB_BYTES,4,0},
};
static int range(uintptr_t p,uint32_t n){return p&&n&&n<=UINTPTR_MAX-p;}
static int overlap(uintptr_t p,uint32_t n,uintptr_t q,uint32_t m){return p<q+m&&q<p+n;}
static int valid(const PeaqDoomPgaState *s){return s&&s->magic==MAGIC;}
static void fail(PeaqDoomPgaState *s,uint32_t error){if(!s->error)s->error=error;s->phase=PDP_FAILED;}
static int location(const PgaApi *api,PeaqDoomPgaState *s){
 if(!api||api->version!=5||api->width!=240||api->height!=320||
    (api->capabilities&(PGA_CAP_BACK|PGA_CAP_RAW_KEYS))!=(PGA_CAP_BACK|PGA_CAP_RAW_KEYS)||
    !api->fill||!api->text){fail(s,PDP_API);return 0;}
 const PeaqProposedPga5Api *x=(const void *)api;
 if(x->bytes!=sizeof(*x)||x->extension_version!=1||!x->location||
    ((uintptr_t)x->location&3u)||!range((uintptr_t)x->location,sizeof(*x->location))){fail(s,PDP_API);return 0;}
 const PeaqProposedGameLocation *p=x->location;
 if(p->bytes!=sizeof(*p)||p->version!=1||(p->drive!='D'&&p->drive!='E')||
    p->reserved[0]||p->reserved[1]||p->reserved[2]){fail(s,PDP_LOCATION);return 0;}
 /* Current qualified native host deliberately permits exactly one folder.
  * Validate the complete bounded UTF16 component before copying; storage is
  * opaque and is never inspected or cast to a private launcher Context. */
 static const char required[]="Doom";
 for(unsigned i=0;i<sizeof(required);i++)if(p->folder[i]!=(uint8_t)required[i]){fail(s,PDP_LOCATION);return 0;}
 s->drive=(char)p->drive;for(unsigned i=0;i<sizeof(required);i++)s->folder[i]=required[i];return 1;
}
int peaq_doom_pga_start(const PgaApi *api,void *state,const PeaqPga5sMemory *m,uint32_t count,uint32_t generation){
 PeaqDoomPgaState *s=state;if(!s||s->magic)return 0;
 s->magic=MAGIC;s->phase=PDP_VALIDATING;s->generation=generation;
 if(!generation){fail(s,PDP_SEGMENTS);return 0;}if(!location(api,s))return 0;
 if(!m||count!=7){fail(s,PDP_SEGMENTS);return 0;}
 const PeaqPga5sMemory *found[7]={0};
 for(unsigned i=0;i<count;i++){
  unsigned j;for(j=0;j<7;j++)if(m[i].id==peaq_doom_pga_segments[j].id)break;
  uintptr_t p=(uintptr_t)m[i].data;
  if(j==7||found[j]||m[i].bytes!=peaq_doom_pga_segments[j].bytes||m[i].alignment!=peaq_doom_pga_segments[j].alignment||
     !range(p,m[i].bytes)||(p&(m[i].alignment-1u))||overlap(p,m[i].bytes,(uintptr_t)s,sizeof(*s))){fail(s,PDP_SEGMENTS);return 0;}
  for(unsigned k=0;k<i;k++)if(overlap(p,m[i].bytes,(uintptr_t)m[k].data,m[k].bytes)){fail(s,PDP_SEGMENTS);return 0;}
  found[j]=&m[i];
 }
 PeaqDoomMemory memory={found[0]->data,found[0]->bytes,found[1]->data,found[1]->bytes,
  found[2]->data,found[2]->bytes,found[6]->data,found[6]->bytes,found[5]->data,found[5]->bytes};
 PeaqDoomMocorHost *host=found[4]->data;PeaqDoomWorker *worker=found[3]->data;
 if(!peaq_doom_mocor_prepare(host,&peaq_pmp122_doom_native,&peaq_pmp122_files,worker,&memory,s->drive)){fail(s,PDP_HOST);return 0;}
 /* Publish the outer gate before any subsequent operation could fail. */
 s->host=host;s->worker=worker;
 if(!peaq_doom_worker_prepare(worker,generation,peaq_doom_mocor_services(host),&memory,s->drive,s->folder,"freedoom1.wad",1,1)){
  fail(s,PDP_WORKER);peaq_doom_mocor_close(host);return 0;
 }
 s->phase=PDP_READY;
 if(peaq_doom_worker_start(worker)!=1){fail(s,PDP_START);peaq_doom_mocor_close(host);return 0;}
 return 1;
}
void peaq_doom_pga_close(const PgaApi *api,void *state){
 (void)api;PeaqDoomPgaState *s=state;if(!valid(s))return;
 s->held=0;s->paused=1;s->phase=PDP_CLOSING;if(s->host)peaq_doom_mocor_close(s->host);
}
int peaq_doom_pga_poll(const PgaApi *api,void *state,uint32_t milliseconds){
 (void)api;(void)milliseconds;PeaqDoomPgaState *s=state;
 if(!s)return 0;if(!s->magic)return 1;if(!valid(s))return 0;
 if(!s->host)return s->phase==PDP_FAILED||s->phase==PDP_CLOSING;
 /* Native counter, not a tick accumulator. One request when due, even after
  * delayed/duplicate callbacks. Close makes service immediately due. The
  * resident candidate owns the5ms timer; native I/O duration is unbounded. */
 uint32_t next_delay=0;
 return peaq_doom_mocor_service(s->host,peaq_mocor_milliseconds(),&next_delay)==1;
}
u32 pga_event(const PgaApi *api,void *state,u32 event,u32 value){
 PeaqDoomPgaState *s=state;if(!valid(s)||s->phase!=PDP_READY||!s->worker)return 0;
 if(event==PGA_PAUSE){s->paused=1;s->held=0;peaq_doom_worker_pause(s->worker,1);return 0;}
 if(event==PGA_RESUME){s->paused=2;s->held=0;return PGA_REDRAW;}
 if(event==PGA_RAW_KEY){
  unsigned kind=value&0xff00,key=value&255;
  if(key==0x17&&(kind==0xfb00||kind==0xfd00))return PGA_QUIT;
  if(s->paused){
   if(s->paused==2&&kind==0xfa00&&(key==0x16||key==0x1b||key==0x26)){
    s->paused=0;peaq_doom_worker_pause(s->worker,0);return PGA_REDRAW;
   }return 0;
  }
  uint32_t bit=peaq_doom_control_bit(key);if(!bit)return 0;
  if(kind==0xfa00)s->held|=bit;else if(kind==0xfb00||kind==0xfd00)s->held&=~bit;else return 0;
  peaq_doom_worker_buttons(s->worker,peaq_doom_control_actions(s->held));return 0;
 }
 if(event==PGA_TICK){
  uint32_t now=peaq_mocor_milliseconds();
  if(s->paint_armed&&(uint32_t)(now-s->paint_at)<50u)return 0;
  s->paint_armed=1;s->paint_at=now;return PGA_REDRAW;
 }
 if(event==PGA_PAINT){
  if(s->paused){api->fill(0,0,240,320,0);api->text(16,24,0xffff,"Pause");return 0;}
  PeaqDoomFrame frame;
  if(peaq_doom_worker_frame_acquire(s->worker,&frame)){
   int result=peaq_display_present(&peaq_pmp122_display,frame.pixels,240,320);
   /* Release even on display failure; no native surface/source is retained. */
   if(!peaq_doom_worker_frame_release(s->worker,&frame)){fail(s,PDP_LEASE);return PGA_QUIT;}
   if(result!=PEAQ_DISPLAY_OK){s->display_error=(uint32_t)result;fail(s,PDP_DISPLAY);return PGA_QUIT;}
   s->paints++;return 0;
  }
  if(!s->paints){api->fill(0,0,240,320,0);api->text(16,24,0xffff,"DOOM");api->text(16,64,0xffff,"Wird geladen...");}
 }
 return 0;
}
