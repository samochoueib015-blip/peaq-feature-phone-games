#include "io_bridge.h"
#include <stdatomic.h>
#include <stddef.h>

#define BRIDGE_MAGIC 0x3142494cu
typedef struct {
 uint32_t magic;
 PeaqLicarChannel channel;
 PeaqLicarIoHost host;PeaqLicarIoMemory memory;PeaqLicarServices services;
 PeaqLicarRuntime *runtime;void (*entry)(void *);void *entry_arg;
 uint32_t handle,started,pool_sequence;
 _Atomic uint32_t clock,error,finish_seen,pool_claims,cleanup_attempts;
 PeaqLicarRequest request;PeaqLicarResponse response;
 uint8_t data_cache[512],music_cache[4096];
 uint32_t data_pos,data_size,music_pos,music_size;
} Bridge;
_Static_assert(sizeof(Bridge)<=PEAQ_LICAR_IO_BRIDGE_BYTES,"Bridge storage");
_Static_assert(ATOMIC_INT_LOCK_FREE==2,"Bridge requires lock-free atomics");
static Bridge *bridge(PeaqLicarIoBridge *b){return (Bridge *)(void *)b;}
static int valid(const Bridge *b){return b&&b->magic==BRIDGE_MAGIC;}
static void fail(Bridge *b,uint32_t error){uint32_t zero=0;atomic_compare_exchange_strong(&b->error,&zero,error);peaq_licar_runtime_stop(b->runtime);peaq_licar_channel_close(&b->channel);}
static int rpc(Bridge *b,uint32_t op,uint32_t arg0,uint32_t arg1,const void *input,
 uint32_t bytes,void *output,uint32_t limit,int32_t *value,uint32_t *got){
 PeaqLicarTicket ticket;*value=0;*got=0;
 int result=peaq_licar_channel_submit(&b->channel,op,arg0,arg1,input,bytes,limit,&ticket);
 if(result!=PLIC_DONE){if(result!=PLIC_CANCELED)fail(b,PLIO_REQUEST);return 0;}
 for(;;){
  result=peaq_licar_channel_result(&b->channel,ticket,value,output,limit,got);
  if(result==PLIC_DONE)return 1;
  if(result!=PLIC_PENDING){*value=0;*got=0;if(result!=PLIC_CANCELED)fail(b,PLIO_RESPONSE);return 0;}
  b->host.park(b->host.cookie,1);
 }
}
static void entry(void *cookie){Bridge *b=cookie;b->entry(b->entry_arg);peaq_licar_channel_worker_done(&b->channel);}
static uint32_t start(void *cookie,void (*run)(void *),void *arg,uint32_t stack_bytes){
 Bridge *b=cookie;if(b->started)return 0;
 if(stack_bytes!=PEAQ_LICAR_WORKER_STACK||peaq_licar_channel_closed(&b->channel)){peaq_licar_channel_no_worker(&b->channel);return 0;}
 b->entry=run;b->entry_arg=arg;b->started=1;
 b->handle=b->host.start(b->host.cookie,entry,b,(uint8_t *)b->memory.tcb+8,188,b->memory.stack,PEAQ_LICAR_WORKER_STACK);
 if(!b->handle)peaq_licar_channel_no_worker(&b->channel);return b->handle;
}
static int join(void *cookie,uint32_t handle){Bridge *b=cookie;return handle==b->handle?peaq_licar_channel_join(&b->channel,b->host.join,b->host.cookie,handle):-1;}
static uint32_t milliseconds(void *cookie){Bridge *b=cookie;return atomic_load(&b->clock);}
static void sleep_ms(void *cookie,uint32_t n){Bridge *b=cookie;b->host.park(b->host.cookie,n);}
static int module_open(void *cookie){Bridge *b=cookie;int32_t value;uint32_t got;return rpc(b,PLIC_MODULE_OPEN,0,0,0,0,0,0,&value,&got)&&value==1;}
static int module_read(void *cookie,void *out,uint32_t n,uint32_t *got){
 Bridge *b=cookie;int32_t value;*got=0;if(!n||n>PEAQ_LICAR_IO_PAYLOAD)return 0;
 return rpc(b,PLIC_MODULE_READ,n,0,0,0,out,n,&value,got)&&value==1;
}
static int module_close(void *cookie){Bridge *b=cookie;int32_t value;uint32_t got;return rpc(b,PLIC_MODULE_CLOSE,0,0,0,0,0,0,&value,&got)&&value==1;}
static void *allocate(void *cookie,uint32_t n){
 Bridge *b=cookie;void *p=0;uint32_t bit=0;
 if(!b->pool_sequence&&n&&n<=b->memory.module_bytes){p=b->memory.module;bit=1;}
 else if(b->pool_sequence==1&&n==PEAQ_LICAR_IO_FRAME_BYTES){p=b->memory.frames;bit=2;}
 if(!p){fail(b,PLIO_POOL);return 0;}
 b->pool_sequence++;atomic_fetch_or(&b->pool_claims,bit);return p;
}
static void release(void *cookie,void *p){
 Bridge *b=cookie;uint32_t bit=p==b->memory.module?1:p==b->memory.frames?2:0;
 if(!bit||!(atomic_fetch_and(&b->pool_claims,~bit)&bit))fail(b,PLIO_POOL);
 /* Logical release only. The UI allocated and retains the entire arena. */
}
static void sync_code(void *cookie,void *p,uint32_t n){
 Bridge *b=cookie;uintptr_t base=(uintptr_t)b->memory.module,at=(uintptr_t)p;int32_t value;uint32_t got;
 if(at<base||at-base>b->memory.module_bytes||n>b->memory.module_bytes-(at-base)){fail(b,PLIO_SYNC);return;}
 if(!rpc(b,PLIC_SYNC_CODE,(uint32_t)(at-base),n,0,0,0,0,&value,&got)){
  /* Canceled sync must still prevent module execution; cancellation itself
   * is not evidence that the native cache operation failed. */
  peaq_licar_runtime_stop(b->runtime);return;
 }
 if(value!=1)fail(b,PLIO_SYNC);
}
static char data_next(void *cookie){
 Bridge *b=cookie;if(peaq_licar_channel_closed(&b->channel))return 0;
 if(b->data_pos==b->data_size){
  int32_t value;uint32_t got;b->data_pos=b->data_size=0;
  if(!rpc(b,PLIC_DATA_READ,sizeof(b->data_cache),0,0,0,b->data_cache,sizeof(b->data_cache),&value,&got)||value!=1||!got)return 0;
  b->data_size=got;
 }
 return (char)b->data_cache[b->data_pos++];
}
static int data_append(void *cookie,const char *text){
 Bridge *b=cookie;uint32_t n=0;if(!text)return 0;
 while(n<PEAQ_LICAR_IO_PAYLOAD&&text[n])n++;
 if(n==PEAQ_LICAR_IO_PAYLOAD)return 0;
 /* Licar uses an empty append as rewind. Invalidate prefetched bytes too;
  * merely returning success here leaves the next menu scan at an old offset. */
 b->data_pos=b->data_size=0;
 int32_t value;uint32_t got;return rpc(b,PLIC_DATA_APPEND,0,0,text,n,0,0,&value,&got)&&value==1;
}
static uint32_t music_read(void *cookie,uint8_t *out,uint32_t n){
 Bridge *b=cookie;if(!out||n>128||peaq_licar_channel_closed(&b->channel))return 0;
 uint32_t copied=0;
 while(copied<n){
  if(b->music_pos==b->music_size){
   int32_t value;uint32_t got;b->music_pos=b->music_size=0;
   if(!rpc(b,PLIC_MUSIC_READ,sizeof(b->music_cache),0,0,0,b->music_cache,sizeof(b->music_cache),&value,&got)||value!=1||!got)break;
   b->music_size=got;
  }
  out[copied++]=b->music_cache[b->music_pos++];
 }
 return copied;
}
static uint32_t audio_submit(void *cookie,const int16_t *pcm,uint32_t n){
 Bridge *b=cookie;if(!pcm||!n||n>800)return 0;int32_t value;uint32_t got;
 if(!rpc(b,PLIC_AUDIO_WRITE,n,0,pcm,n*2,0,0,&value,&got))return 0;
 if(value<0||(uint32_t)value>n){fail(b,PLIO_RESPONSE);return 0;}return (uint32_t)value;
}
static void finish(void *cookie){Bridge *b=cookie;atomic_store(&b->finish_seen,1);}
static int range(uintptr_t p,uint32_t n){return p&&!(p&3)&&n&&p+n>=p;}
static int release_ready(Bridge *b){
 PeaqLicarStatus status;peaq_licar_runtime_status(b->runtime,&status);
 return status.state==PLR_REAPED&&!atomic_load(&b->pool_claims)&&peaq_licar_channel_can_release(&b->channel);
}
int peaq_licar_io_prepare(PeaqLicarIoBridge *storage,uint32_t generation,PeaqLicarRuntime *runtime,
 const PeaqLicarIoHost *host,const PeaqLicarIoMemory *memory){
 Bridge *b=bridge(storage);if(!b||!runtime||!host||!memory||!generation||!host->start||!host->join||!host->park||!host->perform||!host->cleanup)return 0;
 if(memory->module_bytes<PEAQ_LICAR_IO_MODULE_BYTES||memory->frame_bytes<PEAQ_LICAR_IO_FRAME_BYTES||
    memory->tcb_bytes<PEAQ_LICAR_IO_TCB_BYTES||memory->stack_bytes<PEAQ_LICAR_WORKER_STACK||((uintptr_t)memory->stack&7))return 0;
 uintptr_t addresses[6]={(uintptr_t)b,(uintptr_t)runtime,(uintptr_t)memory->module,(uintptr_t)memory->frames,(uintptr_t)memory->tcb,(uintptr_t)memory->stack};
 uint32_t sizes[6]={PEAQ_LICAR_IO_BRIDGE_BYTES,PEAQ_LICAR_RUNTIME_BYTES,memory->module_bytes,memory->frame_bytes,memory->tcb_bytes,memory->stack_bytes};
 for(unsigned i=0;i<6;i++){if(!range(addresses[i],sizes[i]))return 0;for(unsigned j=0;j<i;j++)if(addresses[i]<addresses[j]+sizes[j]&&addresses[j]<addresses[i]+sizes[i])return 0;}
 PeaqLicarStatus status;peaq_licar_runtime_status(runtime,&status);
 if((status.state!=PLR_EMPTY&&status.state!=PLR_REAPED)||(status.state==PLR_EMPTY&&runtime->alignment))return 0;
 if(b->magic){PeaqLicarChannelStatus old;peaq_licar_channel_status(&b->channel,&old);if(!valid(b)||generation<=old.generation||!release_ready(b))return 0;}
 PeaqLicarIoHost saved_host=*host;PeaqLicarIoMemory saved_memory=*memory;
 *b=(Bridge){0};b->magic=BRIDGE_MAGIC;b->runtime=runtime;b->host=saved_host;b->memory=saved_memory;
 if(!peaq_licar_channel_init(&b->channel,generation))return 0;
 for(uint32_t i=0;i<PEAQ_LICAR_IO_TCB_BYTES;i++)((uint8_t *)b->memory.tcb)[i]=0;
 b->services=(PeaqLicarServices){b,start,join,milliseconds,sleep_ms,module_open,module_read,module_close,
                              allocate,release,sync_code,data_next,data_append,music_read,audio_submit,finish};
 return 1;
}
const PeaqLicarServices *peaq_licar_io_services(PeaqLicarIoBridge *storage){Bridge *b=bridge(storage);return valid(b)?&b->services:0;}
static int request_valid(const Bridge *b,const PeaqLicarRequest *r){
 switch(r->operation){
 case PLIC_MODULE_OPEN:case PLIC_MODULE_CLOSE:return !r->bytes&&!r->output_limit&&!r->arg0&&!r->arg1;
 case PLIC_MODULE_READ:case PLIC_DATA_READ:case PLIC_MUSIC_READ:
  return !r->bytes&&!r->arg1&&r->arg0&&r->arg0<=PEAQ_LICAR_IO_PAYLOAD&&r->output_limit==r->arg0;
 case PLIC_DATA_APPEND:return !r->output_limit&&!r->arg0&&!r->arg1;
 case PLIC_AUDIO_WRITE:return r->arg0&&r->arg0<=800&&r->bytes==r->arg0*2&&!r->arg1&&!r->output_limit;
 case PLIC_SYNC_CODE:return !r->bytes&&!r->output_limit&&r->arg0<=b->memory.module_bytes&&r->arg1<=b->memory.module_bytes-r->arg0;
 default:return 0;
 }
}
int peaq_licar_io_pump(PeaqLicarIoBridge *storage,uint32_t now){
 Bridge *b=bridge(storage);if(!valid(b))return 0;atomic_store(&b->clock,now);
 if(peaq_licar_channel_take(&b->channel,&b->request)==PLIC_DONE){
  b->response.value=0;b->response.bytes=0;
  if(!peaq_licar_channel_closed(&b->channel)){
   if(!request_valid(b,&b->request))fail(b,PLIO_REQUEST);
   else if(!b->host.perform(b->host.cookie,&b->request,&b->response))fail(b,PLIO_BACKEND);
  }
  if(b->response.bytes>b->request.output_limit){fail(b,PLIO_RESPONSE);b->response.bytes=0;}
  peaq_licar_channel_respond(&b->channel,b->request.ticket,b->response.value,b->response.data,b->response.bytes);
 }
 PeaqLicarChannelStatus status;peaq_licar_channel_status(&b->channel,&status);
 if(status.worker_done&&!status.drained&&peaq_licar_channel_idle(&b->channel)){
  atomic_fetch_add(&b->cleanup_attempts,1);int cleaned=b->host.cleanup(b->host.cookie);
  if(cleaned==1)peaq_licar_channel_drained(&b->channel);
  else if(cleaned<0){uint32_t zero=0;atomic_compare_exchange_strong(&b->error,&zero,PLIO_CLEANUP);}
 }
 peaq_licar_runtime_poll(b->runtime);return release_ready(b);
}
void peaq_licar_io_close(PeaqLicarIoBridge *storage){Bridge *b=bridge(storage);if(valid(b)){peaq_licar_runtime_stop(b->runtime);peaq_licar_channel_close(&b->channel);}}
void peaq_licar_io_status(const PeaqLicarIoBridge *storage,PeaqLicarIoStatus *out){
 const Bridge *b=(const Bridge *)(const void *)storage;if(!out)return;*out=(PeaqLicarIoStatus){0};if(!valid(b))return;
 peaq_licar_channel_status(&b->channel,&out->channel);out->error=atomic_load(&b->error);out->finish_seen=atomic_load(&b->finish_seen);
 out->cleanup_attempts=atomic_load(&b->cleanup_attempts);out->pool_claims=atomic_load(&b->pool_claims);out->started=b->started;
}
