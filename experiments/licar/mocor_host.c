#include "mocor_host.h"
#include <stdatomic.h>
#include <stddef.h>

#define MAGIC 0x31484d4cu
static const char worker_name[]="PEAQ Licar";
const PeaqLicarMocorApi peaq_pmp122_licar_native={
 (PeaqTxCreate)PEAQ_TX_CREATE_THUMB,(PeaqTxThreadOp)PEAQ_TX_RESUME_THUMB,
 (PeaqTxThreadOp)0x80a07ff1u,(PeaqTxInfo)PEAQ_TX_INFO_THUMB,
 (PeaqTxThreadOp)PEAQ_TX_DELETE_THUMB,(void *)0x80a8441du,peaq_licar_mocor_sync_code
};
typedef struct { uint32_t handle,position,size,attempted,poisoned,close_uncertain; } File;
typedef struct {
 uint32_t magic;
 PeaqLicarMocorApi api;PeaqFileApi files;PeaqLicarIoHost services;
 PeaqLicarIoBridge *bridge;PeaqLicarIoMemory memory;PeaqResidentTxJoin join;
 void (*entry)(void *);void *arg;
 uint32_t attempted,phase,unsupported,read_calls,read_bytes;
 _Atomic uint32_t enabled,entered,error,native_status,park_calls;
 uint16_t paths[3][96];File file[3];
} Host;
_Static_assert(sizeof(Host)<=PEAQ_LICAR_MOCOR_HOST_BYTES,"Mocor host storage");
_Static_assert(ATOMIC_INT_LOCK_FREE==2,"Native worker needs lock-free atomics");
static Host *host(PeaqLicarMocorHost *h){return (Host *)(void *)h;}
static int valid(const Host *h){return h&&h->magic==MAGIC;}
static void failure(Host *h,uint32_t error,uint32_t native){
 uint32_t zero=0;atomic_compare_exchange_strong(&h->error,&zero,error);atomic_store(&h->native_status,native);
}
static int info(Host *h,uint32_t *state,uint32_t *runs){
 char *name=0;uint32_t priority=0,threshold=0,slice=0;void *next=0,*suspended=0;
 uint32_t s=h->api.info(h->join.tcb,&name,state,runs,&priority,&threshold,&slice,&next,&suspended);
 if(s||name!=worker_name){failure(h,PLMH_IDENTITY,s);return 0;}return 1;
}
/* Only an unstarted, exclusively-owned TCB is eligible for rollback. Native
 * terminate is NEVER a runtime cancellation/join operation. Failed rollback
 * retains the arenas; a later UI cleanup may retry the proved-unstarted TCB.
 */
static int rollback(Host *h){
 uint32_t state=UINT32_MAX,runs=UINT32_MAX;
 if(atomic_load(&h->entered)||!info(h,&state,&runs)||runs||
    (state!=PEAQ_THREAD_SUSPENDED&&state!=PEAQ_THREAD_TERMINATED))return -1;
 if(state==PEAQ_THREAD_SUSPENDED){
  uint32_t s=h->api.terminate(h->join.tcb);
  if(s){failure(h,PLMH_ROLLBACK_ERROR,s);return -1;}
 }
 uint32_t s=h->api.delete_thread(h->join.tcb);
 if(s){failure(h,PLMH_ROLLBACK_ERROR,s);return -1;}
 h->phase=PLMH_NO_THREAD;h->join.joined=1;return 1;
}
static void trampoline(uint32_t cookie){
 Host *h=(Host *)(uintptr_t)cookie;atomic_store(&h->entered,1);
 if(atomic_load(&h->enabled))h->entry(h->arg);
}
static uint32_t start(void *cookie,void (*entry)(void *),void *arg,void *tcb,
 uint32_t tcb_bytes,void *stack,uint32_t stack_bytes){
 Host *h=cookie;
 if(!entry||h->attempted||tcb!=(uint8_t *)h->memory.tcb+8||tcb_bytes!=188||
    stack!=h->memory.stack||stack_bytes!=PEAQ_LICAR_WORKER_STACK){failure(h,PLMH_ARGUMENT,0);return 0;}
 h->attempted=1;h->entry=entry;h->arg=arg;
 PeaqResidentTxApi join_api={h->api.info,h->api.delete_thread};
 if(!peaq_resident_tx_join_init(&h->join,&join_api,tcb,worker_name)){failure(h,PLMH_ARGUMENT,0);h->phase=PLMH_NO_THREAD;return 0;}
 uint32_t s=h->api.create(tcb,worker_name,trampoline,(uint32_t)(uintptr_t)h,stack,stack_bytes,160,160,1,0);
 if(s){failure(h,PLMH_CREATE,s);h->phase=PLMH_NO_THREAD;return 0;}
 /* Publish everything before resume: the worker is allowed to run before the
  * native resume call returns to this UI stack. */
 h->phase=PLMH_RUNNING;atomic_store(&h->enabled,1);s=h->api.resume(tcb);
 if(!s)return (uint32_t)(uintptr_t)tcb;
 atomic_store(&h->enabled,0);failure(h,PLMH_RESUME,s);
 uint32_t state=UINT32_MAX,runs=UINT32_MAX;
 if(!atomic_load(&h->entered)&&info(h,&state,&runs)&&!runs&&state==PEAQ_THREAD_SUSPENDED){
  h->phase=PLMH_ROLLBACK;rollback(h);
  /* No engine callback ran, this TCB was never scheduled, and this owner will
   * never resume it. Failed terminate/delete still blocks UI cleanup. */
  return 0;
 }
 /* Unknown/live state cannot satisfy start's zero-handle promise. Retain a
  * nonzero identity, abort the bridge and keep memory pinned. No kill or false
  * worker_done is fabricated; an unresolved instance may require a reboot. */
 h->phase=PLMH_QUARANTINED;peaq_licar_io_close(h->bridge);
 /* If this task can still enter, let the already-canceled bridge wrapper
  * return normally and publish worker_done. A trampoline that already skipped
  * its entry remains quarantined; neither case permits a fabricated join. */
 atomic_store(&h->enabled,1);return (uint32_t)(uintptr_t)tcb;
}
static int join(void *cookie,uint32_t handle){
 Host *h=cookie;int result=peaq_resident_tx_join_poll(&h->join,handle);
 if(result==1)h->phase=PLMH_JOINED;return result;
}
static void park(void *cookie,uint32_t duration){
 Host *h=cookie;if(!duration)duration=1;if(duration>50)duration=50;
 atomic_fetch_add(&h->park_calls,1);uint32_t status=h->api.sleep(duration);
 if(status){failure(h,PLMH_PARK,status);peaq_licar_io_close(h->bridge);}
}
static int close_file(Host *h,unsigned i){
 File *f=&h->file[i];if(f->close_uncertain)return 0;if(!f->handle)return 1;
 uint32_t s=h->files.close(f->handle);
 if(s){f->poisoned=f->close_uncertain=1;failure(h,PLMH_CLOSE,s);return 0;}
 f->handle=0;return 1;
}
static int open_file(Host *h,unsigned i){
 File *f=&h->file[i];if(f->poisoned)return 0;if(f->handle)return 1;if(f->attempted)return 0;
 f->attempted=1;f->handle=h->files.open(h->paths[i],0x31,0,0);
 if(!f->handle){f->poisoned=1;failure(h,PLMH_OPEN,0);return 0;}
 uint32_t size=0,s=h->files.size(f->handle,&size);
 uint32_t cap=i==0?1024u*1024u+400032u:i==1?128u*1024u:8u*1024u*1024u;
 if(s||size>cap||(i==0&&!size)){f->poisoned=1;failure(h,PLMH_SIZE,s);close_file(h,i);return 0;}
 f->size=size;return 1;
}
static int rewind_file(Host *h,unsigned i){
 File *f=&h->file[i];if(f->poisoned)return 0;if(!f->handle){f->position=0;return 1;}
 uint32_t s=h->files.seek(f->handle,0,0);
 if(s){f->poisoned=1;failure(h,PLMH_SEEK,s);close_file(h,i);return 0;}
 f->position=0;return 1;
}
static int read_file(Host *h,unsigned i,uint32_t want,PeaqLicarResponse *out){
 File *f=&h->file[i];if(!open_file(h,i))return 0;
 /* Music loops without an artificial empty chunk. Data must return a zero
  * EOF sentinel once before the next read restarts, matching Licar's ABI. */
 if(f->position==f->size){
  if(i==0)return 1;
  if(!rewind_file(h,i))return 0;
  if(i==1||!f->size)return 1;
 }
 uint32_t count=0,remaining=f->size-f->position;if(want>remaining)want=remaining;
 h->read_calls++;uint32_t s=h->files.read(f->handle,out->data,want,&count,0);
 if(s||count>want||!count){f->poisoned=1;failure(h,PLMH_READ,s);close_file(h,i);return 0;}
 f->position+=count;h->read_bytes+=count;out->bytes=count;return 1;
}
static int perform(void *cookie,const PeaqLicarRequest *r,PeaqLicarResponse *out){
 Host *h=cookie;if(!r||!out)return 0;out->value=0;out->bytes=0;
 if(h->phase!=PLMH_RUNNING||r->bytes>4096||r->output_limit>4096)return 0;
 switch(r->operation){
 case PLIC_MODULE_OPEN:
  if(r->arg0||r->arg1||r->bytes||r->output_limit||h->file[0].attempted)return 0;
  out->value=open_file(h,0);return 1;
 case PLIC_MODULE_CLOSE:
  if(r->arg0||r->arg1||r->bytes||r->output_limit)return 0;
  out->value=close_file(h,0);return 1;
 case PLIC_MODULE_READ:case PLIC_DATA_READ:case PLIC_MUSIC_READ:{
  if(!r->arg0||r->arg0>4096||r->arg1||r->bytes||r->output_limit!=r->arg0)return 0;
  unsigned i=r->operation==PLIC_MODULE_READ?0:r->operation==PLIC_DATA_READ?1:2;
  if(!i&&!h->file[0].handle)return 1;
  out->value=read_file(h,i,r->arg0,out);return 1;}
 case PLIC_DATA_APPEND:
  if(r->arg0||r->arg1||r->output_limit)return 0;
  if(!r->bytes)out->value=rewind_file(h,1);
  else h->unsupported|=PLMH_NO_SAVE;
  return 1;
 case PLIC_AUDIO_WRITE:
  if(!r->arg0||r->arg0>800||r->bytes!=r->arg0*2||r->arg1||r->output_limit)return 0;
  h->unsupported|=PLMH_NO_AUDIO;return 1;
 case PLIC_SYNC_CODE:
  if(r->bytes||r->output_limit||r->arg0>h->memory.module_bytes||r->arg1>h->memory.module_bytes-r->arg0)return 0;
  if(r->arg1)h->api.sync_code((uint8_t *)h->memory.module+r->arg0,r->arg1);
  out->value=1;return 1;
 default:failure(h,PLMH_REQUEST,0);return 0;
 }
}
static int cleanup(void *cookie){
 Host *h=cookie;int clean=1;
 for(unsigned i=0;i<3;i++)if(!close_file(h,i))clean=0;
 if(h->phase==PLMH_ROLLBACK&&rollback(h)!=1)clean=0;
 if(h->phase==PLMH_QUARANTINED)clean=0;
 return clean?1:-1;
}
static int range(void *p,uint32_t n){uintptr_t a=(uintptr_t)p;return a&&!(a&3)&&n&&a+n>=a;}
int peaq_licar_mocor_prepare(PeaqLicarMocorHost *storage,const PeaqLicarMocorApi *api,
 const PeaqFileApi *files,PeaqLicarIoBridge *bridge,const PeaqLicarIoMemory *memory,char drive){
 Host *h=host(storage);
 if(!h||((uintptr_t)h&3)||h->magic||!api||!files||!bridge||!memory||!api->create||!api->resume||!api->terminate||
    !api->info||!api->delete_thread||!api->sleep||!api->sync_code||!files->open||!files->read||!files->close||!files->seek||!files->size)return 0;
 if(!range(memory->module,memory->module_bytes)||memory->module_bytes<PEAQ_LICAR_IO_MODULE_BYTES||
    !range(memory->frames,memory->frame_bytes)||memory->frame_bytes<PEAQ_LICAR_IO_FRAME_BYTES||
    !range(memory->tcb,memory->tcb_bytes)||memory->tcb_bytes<196||
    !range(memory->stack,memory->stack_bytes)||((uintptr_t)memory->stack&7)||memory->stack_bytes<PEAQ_LICAR_WORKER_STACK)return 0;
 void *addresses[]={h,bridge,memory->module,memory->frames,memory->tcb,memory->stack};
 uint32_t sizes[]={PEAQ_LICAR_MOCOR_HOST_BYTES,PEAQ_LICAR_IO_BRIDGE_BYTES,memory->module_bytes,
                   memory->frame_bytes,memory->tcb_bytes,memory->stack_bytes};
 for(unsigned i=0;i<6;i++){
  if(!range(addresses[i],sizes[i]))return 0;
  uintptr_t a=(uintptr_t)addresses[i];
  for(unsigned j=0;j<i;j++){uintptr_t b=(uintptr_t)addresses[j];if(a<b+sizes[j]&&b<a+sizes[i])return 0;}
 }
 uint16_t path[96];if(!peaq_game_file_path(path,drive,"Racer","engine.pge"))return 0;
 *h=(Host){0};h->magic=MAGIC;h->api=*api;h->files=*files;h->bridge=bridge;h->memory=*memory;
 const char *names[]={"engine.pge","data","music.u8"};
 for(unsigned i=0;i<3;i++)peaq_game_file_path(h->paths[i],drive,"Racer",names[i]);
 h->services=(PeaqLicarIoHost){h,start,join,park,perform,cleanup};return 1;
}
const PeaqLicarIoHost *peaq_licar_mocor_services(PeaqLicarMocorHost *storage){Host *h=host(storage);return valid(h)?&h->services:0;}
void peaq_licar_mocor_status(const PeaqLicarMocorHost *storage,PeaqLicarMocorStatus *out){
 const Host *h=(const Host *)(const void *)storage;if(!out)return;*out=(PeaqLicarMocorStatus){0};if(!valid(h))return;
 out->phase=h->phase;out->error=atomic_load(&h->error);out->native_status=atomic_load(&h->native_status);
 out->entered=atomic_load(&h->entered);out->unsupported=h->unsupported;out->park_calls=atomic_load(&h->park_calls);
 out->read_calls=h->read_calls;out->read_bytes=h->read_bytes;
 for(unsigned i=0;i<3;i++){out->open_handles+=!!h->file[i].handle;out->close_uncertain+=!!h->file[i].close_uncertain;}
}
int peaq_licar_mocor_quiescent(const PeaqLicarMocorHost *storage){
 const Host *h=(const Host *)(const void *)storage;
 if(!valid(h)||(h->phase!=PLMH_IDLE&&h->phase!=PLMH_NO_THREAD&&h->phase!=PLMH_JOINED))return 0;
 for(unsigned i=0;i<3;i++)if(h->file[i].handle||h->file[i].close_uncertain)return 0;return 1;
}
