#include "worker.h"
#include "platform.h"
#include "guard.h"
#include "read_cache.h"
#include "../runtime/module.h"
#include <stdatomic.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>

#define MAGIC 0x31574450u
#define PIXELS (240u*320u)
#define PAYLOAD PEAQ_LICAR_IO_PAYLOAD
enum { FREE,WRITING,READY,READING };
typedef struct {
 uint32_t magic,generation;
 PeaqDoomWorkerHost host; PeaqDoomMemory memory;
 PeaqFileApi native,ui_api; PeaqDoomFiles files;
 PeaqLicarChannel channel; PeaqLicarRequest request; PeaqLicarResponse response;
 PeaqDoomReadCache read_cache;
 _Atomic uint32_t state,error,stop,paused,held,edges,clock;
 _Atomic uint32_t frames,steps,buttons,heap_used,exit_seen,exit_status,close_uncertain;
 uint32_t handle,pumping,ui_now,clock_seen,cleanup_calls;
 uint32_t lease,lease_sequence,module_claimed; int32_t module_fd;
 PeaqModule module; DoomHost doom_host;
 uint16_t *portrait,*published[2];
 _Atomic uint32_t slot_state[2]; uint32_t slot_sequence[2];
 char iwad[32],episode[2],map[2];
} Worker;
_Static_assert(sizeof(Worker)<=PEAQ_DOOM_WORKER_BYTES,"Doom worker storage");
_Static_assert(ATOMIC_INT_LOCK_FREE==2,"Doom worker requires lock-free atomics");
static _Atomic(Worker *) active;
static Worker *worker(PeaqDoomWorker *w){return (Worker *)(void *)w;}
static int valid(const Worker *w){return w&&w->magic==MAGIC;}
static void set_error(Worker *w,uint32_t e){uint32_t zero=0;atomic_compare_exchange_strong(&w->error,&zero,e);}
static void clear_keys(Worker *w){atomic_store(&w->held,0);atomic_store(&w->edges,0);}
void peaq_doom_worker_close(PeaqDoomWorker *p){
 Worker *w=worker(p);if(!valid(w))return;
 atomic_store(&w->stop,1);clear_keys(w);peaq_licar_channel_close(&w->channel);
 uint32_t prepared=PDW_PREPARED;
 if(atomic_compare_exchange_strong(&w->state,&prepared,PDW_FINISHED))
  peaq_licar_channel_no_worker(&w->channel);
}
static void fail(Worker *w,uint32_t error){set_error(w,error);peaq_doom_worker_close((PeaqDoomWorker *)w);}
static int name_length(const char *s){
 if(!s||!s[0]||s[0]=='.')return 0;
 for(uint32_t n=0;n<32;n++){
  unsigned c=(unsigned char)s[n];if(!c)return (int)n+1;
  if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return 0;
 }
 return 0;
}
static void park(Worker *w,uint32_t n){if(!n)n=1;if(n>50)n=50;w->host.park(w->host.cookie,n);}
static int rpc(Worker *w,uint32_t op,uint32_t arg0,uint32_t arg1,const void *in,
 uint32_t bytes,void *out,uint32_t limit,int32_t *value,uint32_t *got){
 PeaqLicarTicket ticket;*value=-EIO;*got=0;
 int result=peaq_licar_channel_submit(&w->channel,op,arg0,arg1,in,bytes,limit,&ticket);
 if(result!=PLIC_DONE){if(result!=PLIC_CANCELED)fail(w,PDW_REQUEST);return 0;}
 for(;;){
  result=peaq_licar_channel_result(&w->channel,ticket,value,out,limit,got);
  if(result==PLIC_DONE)return 1;
  if(result!=PLIC_PENDING){*value=-EIO;*got=0;if(result!=PLIC_CANCELED)fail(w,PDW_RESPONSE);return 0;}
  park(w,1);
 }
}
static int simple(Worker *w,uint32_t op){int32_t value;uint32_t got;return rpc(w,op,0,0,0,0,0,0,&value,&got)&&value==1;}
static int stream_read(void *cookie,void *out,uint32_t n,uint32_t *got){
 Worker *w=cookie;int32_t value;*got=0;
 return n&&n<=PAYLOAD&&rpc(w,PLIC_MODULE_READ,n,0,0,0,out,n,&value,got)&&value==1;
}
static void *module_alloc(uint32_t n){
 Worker *w=atomic_load(&active);
 if(!w||w->module_claimed||!n||n>PEAQ_DOOM_MODULE_BYTES)return 0;
 w->module_claimed=1;return w->memory.module;
}
static void module_free(void *p){
 Worker *w=atomic_load(&active);if(p!=w->memory.module||!w->module_claimed){fail(w,PDW_MODULE);return;}
 w->module_claimed=0; /* logical only: the UI retains every allocation */
}
static void module_sync(void *p,uint32_t n){
 Worker *w=atomic_load(&active);uintptr_t at=(uintptr_t)p,base=(uintptr_t)w->memory.module;
 int32_t value;uint32_t got;
 if(at<base||at-base>w->memory.module_bytes||n>w->memory.module_bytes-(at-base)){fail(w,PDW_SYNC);return;}
 if(!rpc(w,PLIC_SYNC_CODE,(uint32_t)(at-base),n,0,0,0,0,&value,&got)||value!=1)fail(w,PDW_SYNC);
}
static int32_t engine_remote_io(void *cookie,uint32_t op,uint32_t a,uint32_t b,uint32_t c){
 Worker *w=cookie;int32_t value;uint32_t got;
 if(op==DOOM_IO_EXIT){atomic_store(&w->exit_status,a);atomic_fetch_add(&w->exit_seen,1);return 0;}
 if(op==DOOM_IO_WRITE){if((a==1||a==2)&&c<=INT_MAX&&(!c||b))return (int32_t)c;return -EROFS;}
 if(op==DOOM_IO_UNLINK||op==DOOM_IO_RENAME||op==DOOM_IO_MKDIR)return -EROFS;
 if(op>DOOM_IO_EXIT)return -ENOSYS;
 if(op==DOOM_IO_OPEN||op==DOOM_IO_STAT){
  const char *name=(const char *)(uintptr_t)a;int n=name_length(name);if(!n)return -EACCES;
  return rpc(w,PLIC_DATA_READ,op,op==DOOM_IO_OPEN?b:0,name,(uint32_t)n,0,0,&value,&got)?value:-EIO;
 }
 if(op==DOOM_IO_READ){
  if(c>INT_MAX||(!b&&c))return -EINVAL;
  uint32_t done=0;
  while(done<c){
   uint32_t n=c-done;if(n>PAYLOAD)n=PAYLOAD;
   if(!rpc(w,PLIC_DATA_READ,op,a,0,0,(uint8_t *)(uintptr_t)b+done,n,&value,&got))return done?(int32_t)done:-EIO;
   if((value<0&&got)||(value>=0&&(uint32_t)value!=got)){fail(w,PDW_RESPONSE);return done?(int32_t)done:-EIO;}
   if(value<0)return done?(int32_t)done:value;
   done+=got;if(got<n)break;
  }
  return (int32_t)done;
 }
 if(op==DOOM_IO_SEEK){uint32_t seek[2]={b,c};return rpc(w,PLIC_DATA_READ,op,a,seek,sizeof(seek),0,0,&value,&got)?value:-EIO;}
 if(op==DOOM_IO_CLOSE||op==DOOM_IO_FSTAT)return rpc(w,PLIC_DATA_READ,op,a,0,0,0,0,&value,&got)?value:-EIO;
 return -ENOSYS;
}
static int32_t engine_io(uint32_t op,uint32_t a,uint32_t b,uint32_t c){
 Worker *w=atomic_load(&active);return peaq_doom_read_cache_io(&w->read_cache,op,a,b,c);
}
static uint32_t milliseconds(void){Worker *w=atomic_load(&active);return atomic_load(&w->clock);}
static void sleep_ms(uint32_t n){park(atomic_load(&active),n);}
static void present(const uint16_t *p,int width,int height){
 Worker *w=atomic_load(&active);
 if(p!=w->portrait||width!=240||height!=320){fail(w,PDW_FRAME);return;}
 if(atomic_load(&w->stop)||atomic_load(&w->paused))return;
 if(atomic_load(&w->frames)==UINT32_MAX){fail(w,PDW_FRAME);return;}
 for(unsigned i=0;i<2;i++){
  uint32_t state=atomic_load(&w->slot_state[i]);
  if(state!=FREE&&state!=READY)continue;
  if(!atomic_compare_exchange_strong(&w->slot_state[i],&state,WRITING))continue;
  for(uint32_t j=0;j<PIXELS;j++)w->published[i][j]=p[j];
  w->slot_sequence[i]=atomic_fetch_add(&w->frames,1)+1;
  atomic_store_explicit(&w->slot_state[i],READY,memory_order_release);return;
 }
}
static void entry(void *cookie){
 Worker *w=cookie;int result=PEAQ_DOOM_BAD_STATE,created=0;
 atomic_store(&w->state,PDW_LOADING);
 if(atomic_load(&w->stop))goto done;
 if(!simple(w,PLIC_MODULE_OPEN)){if(!atomic_load(&w->stop))set_error(w,PDW_MODULE);goto done;}
 PeaqModuleHost loader={w,stream_read,module_alloc,module_free,module_sync};
 int loaded=peaq_module_load(&loader,&w->module);
 int closed=simple(w,PLIC_MODULE_CLOSE);
 if(atomic_load(&w->stop))goto done;
 if(loaded!=1||!closed||w->module.export_count!=10){set_error(w,PDW_MODULE);goto done;}
 uint32_t *e=w->module.exports;
 uint32_t (*abi)(void)=(void *)(uintptr_t)e[9];
 int (*bind)(const DoomHost *,uint16_t *)=(void *)(uintptr_t)e[2];
 int (*io_bind)(int32_t (*)(uint32_t,uint32_t,uint32_t,uint32_t),void *,void *)=(void *)(uintptr_t)e[5];
 w->doom_host=(DoomHost){milliseconds,sleep_ms,present};
 uintptr_t heap=((uintptr_t)w->memory.heap+7u)&~(uintptr_t)7u;
 if(abi()!=1||bind(&w->doom_host,w->portrait)!=1||io_bind(engine_io,(void *)heap,(void *)(heap+2560u*1024u))!=1){set_error(w,PDW_ENGINE);goto done;}
 char *argv[]={"peaq-doom","-iwad",w->iwad,"-warp",w->episode,w->map,"-nosound","-nomouse","-mb","2",0};
 int (*create)(int,char **)=(void *)(uintptr_t)e[0];created=1;result=create(10,argv);
 if(result==PEAQ_DOOM_RUNNING)atomic_store(&w->state,PDW_RUNNING);
 while(result==PEAQ_DOOM_RUNNING&&!atomic_load(&w->stop)){
  if(atomic_load(&w->paused)){park(w,20);continue;}
  void (*buttons)(uint32_t)=(void *)(uintptr_t)e[3];
  uint32_t bits=atomic_load(&w->held)|atomic_exchange(&w->edges,0);
  atomic_store(&w->buttons,bits);buttons(bits);
  int (*tick)(void)=(void *)(uintptr_t)e[1];result=tick();atomic_fetch_add(&w->steps,1);park(w,1);
 }
 if(result==PEAQ_DOOM_RUNNING){int (*quit)(void)=(void *)(uintptr_t)e[7];result=quit();}
 if(result!=PEAQ_DOOM_EXITED&&!(atomic_load(&w->stop)&&result==PEAQ_DOOM_FAILED))set_error(w,PDW_ENGINE);
 if(created){uint32_t (*used)(void)=(void *)(uintptr_t)e[6];atomic_store(&w->heap_used,used());}
done:
 atomic_store(&w->state,PDW_FINISHED);
 /* The native shell has NOT yet returned: this is only the channel's done
  * publication. Real join remains mandatory, even if the UI runs right now. */
 peaq_licar_channel_worker_done(&w->channel);
}
static uint32_t ui_close(uint32_t handle){
 Worker *w=atomic_load(&active);
 if(!w||atomic_load(&w->close_uncertain))return 1;
 uint32_t status=w->native.close(handle);
 if(status){atomic_store(&w->close_uncertain,1);fail(w,PDW_CLOSE_UNCERTAIN);}
 return status;
}
static int range(uintptr_t p,uint32_t n){return p&&!(p&3)&&n&&p+n>=p;}
int peaq_doom_worker_prepare(PeaqDoomWorker *p,uint32_t generation,const PeaqDoomWorkerHost *host,
 const PeaqDoomMemory *memory,char drive,const char *folder,const char *iwad,uint32_t episode,uint32_t map){
 Worker *w=worker(p);uint16_t checked[96];int iwlen=name_length(iwad);
 /* Even preparation clears its TCB prefix. Do not let a second caller reuse
  * overlapping reservations while an old worker or UI callback is live. */
 if(atomic_load(&active))return 0;
 if(!w||!generation||!host||!memory||!host->start||!host->join||!host->park||!host->sync_code||!host->files||
    !iwlen||episode<1||episode>4||map<1||map>9||!peaq_game_file_path(checked,drive,folder,iwad))return 0;
 if(w->magic&&(!valid(w)||atomic_load(&w->state)!=PDW_REAPED||generation<=w->generation))return 0;
 if(memory->module_bytes<PEAQ_DOOM_MODULE_BYTES||memory->heap_bytes<PEAQ_DOOM_HEAP_BYTES||memory->frame_bytes<PEAQ_DOOM_FRAME_BYTES||
    memory->tcb_bytes<PEAQ_DOOM_TCB_BYTES||memory->stack_bytes<PEAQ_DOOM_STACK_BYTES||((uintptr_t)memory->stack&7))return 0;
 uintptr_t at[6]={(uintptr_t)w,(uintptr_t)memory->module,(uintptr_t)memory->heap,(uintptr_t)memory->frames,(uintptr_t)memory->tcb,(uintptr_t)memory->stack};
 uint32_t size[6]={PEAQ_DOOM_WORKER_BYTES,memory->module_bytes,memory->heap_bytes,memory->frame_bytes,memory->tcb_bytes,memory->stack_bytes};
 for(unsigned i=0;i<6;i++){if(!range(at[i],size[i]))return 0;for(unsigned j=0;j<i;j++)if(at[i]<at[j]+size[j]&&at[j]<at[i]+size[i])return 0;}
 PeaqDoomWorkerHost h=*host;PeaqDoomMemory m=*memory;PeaqFileApi api=*host->files;
 PeaqDoomFiles files={0};if(peaq_doom_files_init(&files,&api,drive,folder))return 0;
 char iw[32];for(int i=0;i<iwlen;i++)iw[i]=iwad[i];
 *w=(Worker){0};w->magic=MAGIC;w->generation=generation;w->host=h;w->memory=m;
 w->native=api;w->ui_api=api;w->ui_api.close=ui_close;w->files=files;w->files.api=&w->ui_api;
 peaq_doom_read_cache_init(&w->read_cache,engine_remote_io,w);
 for(int i=0;i<iwlen;i++)w->iwad[i]=iw[i];w->episode[0]=(char)('0'+episode);w->map[0]=(char)('0'+map);
 w->module_fd=-1;w->portrait=(uint16_t *)(((uintptr_t)m.frames+31u)&~(uintptr_t)31u);
 w->published[0]=w->portrait+PIXELS;w->published[1]=w->published[0]+PIXELS;
 for(uint32_t i=0;i<PEAQ_DOOM_TCB_BYTES;i++)((uint8_t *)m.tcb)[i]=0;
 if(!peaq_licar_channel_init(&w->channel,generation))return 0;
 atomic_store(&w->state,PDW_PREPARED);return 1;
}
int peaq_doom_worker_start(PeaqDoomWorker *p){
 Worker *w=worker(p),*empty=0;if(!valid(w)||atomic_load(&w->state)!=PDW_PREPARED)return 0;
 if(!atomic_compare_exchange_strong(&active,&empty,w))return 0;
 if(atomic_load(&w->stop)){peaq_licar_channel_no_worker(&w->channel);atomic_store(&w->state,PDW_FINISHED);return -1;}
 atomic_store(&w->state,PDW_QUEUED);
 w->handle=w->host.start(w->host.cookie,entry,w,(uint8_t *)w->memory.tcb+8,188,w->memory.stack,PEAQ_DOOM_STACK_BYTES);
 if(!w->handle){set_error(w,PDW_THREAD);peaq_licar_channel_no_worker(&w->channel);atomic_store(&w->state,PDW_FINISHED);return -1;}
 return 1;
}
static int request_valid(const Worker *w,const PeaqLicarRequest *r){
 switch(r->operation){
 case PLIC_MODULE_OPEN:case PLIC_MODULE_CLOSE:return !r->bytes&&!r->output_limit&&!r->arg0&&!r->arg1;
 case PLIC_MODULE_READ:return !r->bytes&&!r->arg1&&r->arg0&&r->arg0<=PAYLOAD&&r->output_limit==r->arg0;
 case PLIC_SYNC_CODE:return !r->bytes&&!r->output_limit&&r->arg0<=w->memory.module_bytes&&r->arg1<=w->memory.module_bytes-r->arg0;
 case PLIC_DATA_READ:
  switch(r->arg0){
  case DOOM_IO_OPEN:case DOOM_IO_STAT:return r->bytes&&r->bytes<=32&&!r->output_limit&&r->data[r->bytes-1]==0&&name_length((const char *)r->data)==(int)r->bytes;
  case DOOM_IO_READ:return !r->bytes&&r->output_limit&&r->output_limit<=PAYLOAD;
  case DOOM_IO_SEEK:return r->bytes==8&&!r->output_limit;
  case DOOM_IO_CLOSE:case DOOM_IO_FSTAT:return !r->bytes&&!r->output_limit;
  default:return 0;
  }
 default:return 0;
 }
}
static void perform(Worker *w){
 PeaqLicarRequest *r=&w->request;PeaqLicarResponse *out=&w->response;
 out->value=-EIO;out->bytes=0;
 if(atomic_load(&w->close_uncertain)||peaq_licar_channel_closed(&w->channel))return;
 if(!request_valid(w,r)){fail(w,PDW_REQUEST);return;}
 uint32_t op=r->arg0,a=r->arg1,b=0,c=0;
 if(r->operation==PLIC_SYNC_CODE){out->value=w->host.sync_code(w->host.cookie,(uint8_t *)w->memory.module+r->arg0,r->arg1);return;}
 if(r->operation==PLIC_MODULE_OPEN){
  if(w->module_fd>=0){fail(w,PDW_REQUEST);return;}
  w->module_fd=peaq_doom_files_io(&w->files,DOOM_IO_OPEN,(uint32_t)(uintptr_t)"engine.pge",0,0);
  out->value=w->module_fd>=0?1:0;return;
 }
 if(r->operation==PLIC_MODULE_READ){op=DOOM_IO_READ;a=(uint32_t)w->module_fd;c=r->output_limit;}
 else if(r->operation==PLIC_MODULE_CLOSE){op=DOOM_IO_CLOSE;a=(uint32_t)w->module_fd;}
 else if(op==DOOM_IO_OPEN||op==DOOM_IO_STAT){a=(uint32_t)(uintptr_t)r->data;b=op==DOOM_IO_OPEN?r->arg1:0;}
 else if(op==DOOM_IO_SEEK){const uint32_t *seek=(const uint32_t *)(const void *)r->data;b=seek[0];c=seek[1];}
 if(op==DOOM_IO_READ){b=(uint32_t)(uintptr_t)out->data;c=r->output_limit;}
 out->value=peaq_doom_files_io(&w->files,op,a,b,c);
 if(op==DOOM_IO_READ&&out->value>=0)out->bytes=(uint32_t)out->value;
 if(r->operation==PLIC_MODULE_READ)out->value=out->value>=0?1:0;
 if(r->operation==PLIC_MODULE_CLOSE){if(!out->value)w->module_fd=-1;out->value=out->value==0?1:0;}
}
int peaq_doom_worker_pump(PeaqDoomWorker *p,uint32_t now){
 Worker *w=worker(p);if(w&&!w->magic)return 1;
 if(!valid(w)||w->pumping)return 0;
 uint32_t state=atomic_load(&w->state);if(state==PDW_REAPED)return 1;
 if(state<PDW_QUEUED)return 0;w->pumping=1;
 if(w->clock_seen&&(!atomic_load(&w->paused)||atomic_load(&w->stop)))atomic_fetch_add(&w->clock,now-w->ui_now);
 w->ui_now=now;w->clock_seen=1;int serviced=0;
 if(peaq_licar_channel_take(&w->channel,&w->request)==PLIC_DONE){
  serviced=1;perform(w);
  if(w->response.bytes>w->request.output_limit){fail(w,PDW_RESPONSE);w->response.bytes=0;}
  peaq_licar_channel_respond(&w->channel,w->request.ticket,w->response.value,w->response.data,w->response.bytes);
 }
 PeaqLicarChannelStatus status;peaq_licar_channel_status(&w->channel,&status);
 if(status.worker_done&&!status.joined){
  int result=peaq_licar_channel_join(&w->channel,w->host.join,w->host.cookie,w->handle);
  if(result<0)set_error(w,PDW_JOIN);
  peaq_licar_channel_status(&w->channel,&status);
 }
 if(status.joined&&!status.drained&&!serviced&&peaq_licar_channel_idle(&w->channel)&&!atomic_load(&w->close_uncertain)){
  unsigned i;for(i=0;i<PEAQ_DOOM_FILE_SLOTS;i++)if(w->files.files[i].handle){
   w->cleanup_calls++;peaq_doom_files_io(&w->files,DOOM_IO_CLOSE,w->files.files[i].fd,0,0);break;
  }
  if(i==PEAQ_DOOM_FILE_SLOTS)peaq_licar_channel_drained(&w->channel);
 }
 int release=!w->lease&&!atomic_load(&w->close_uncertain)&&peaq_licar_channel_can_release(&w->channel);
 if(release){
  w->module_claimed=0;atomic_store(&w->state,PDW_REAPED);
  Worker *owned=w;atomic_compare_exchange_strong(&active,&owned,0);
 }
 w->pumping=0;return release;
}
void peaq_doom_worker_buttons(PeaqDoomWorker *p,uint32_t bits){
 Worker *w=worker(p);if(!valid(w)||atomic_load(&w->paused)||atomic_load(&w->stop))return;
 bits&=0x7efu;uint32_t old=atomic_exchange(&w->held,bits);atomic_fetch_or(&w->edges,bits&~old);
}
void peaq_doom_worker_pause(PeaqDoomWorker *p,int pause){Worker *w=worker(p);if(valid(w)){atomic_store(&w->paused,!!pause);clear_keys(w);}}
void peaq_doom_worker_status(const PeaqDoomWorker *p,PeaqDoomWorkerStatus *out){
 const Worker *w=(const Worker *)(const void *)p;if(!out)return;*out=(PeaqDoomWorkerStatus){0};if(!valid(w))return;
 *out=(PeaqDoomWorkerStatus){atomic_load(&w->state),atomic_load(&w->error),atomic_load(&w->stop),atomic_load(&w->paused),
  atomic_load(&w->frames),atomic_load(&w->steps),atomic_load(&w->buttons),atomic_load(&w->heap_used),
  atomic_load(&w->exit_seen),(int32_t)atomic_load(&w->exit_status),atomic_load(&w->close_uncertain),w->cleanup_calls,{0}};
 peaq_licar_channel_status(&w->channel,&out->channel);
}
int peaq_doom_worker_frame_acquire(PeaqDoomWorker *p,PeaqDoomFrame *out){
 Worker *w=worker(p);if(!out)return 0;*out=(PeaqDoomFrame){0};
 if(!valid(w)||w->lease)return 0;uint32_t phase=atomic_load(&w->state);if(phase<PDW_LOADING||phase>=PDW_REAPED)return 0;
 for(unsigned i=0;i<2;i++){
  uint32_t ready=READY;if(!atomic_compare_exchange_strong_explicit(&w->slot_state[i],&ready,READING,memory_order_acquire,memory_order_relaxed))continue;
  w->lease=i+1;w->lease_sequence=w->slot_sequence[i];*out=(PeaqDoomFrame){w->published[i],w->generation,i+1,w->lease_sequence};return 1;
 }
 return 0;
}
int peaq_doom_worker_frame_release(PeaqDoomWorker *p,const PeaqDoomFrame *frame){
 Worker *w=worker(p);if(!valid(w)||!frame||!w->lease||frame->generation!=w->generation||frame->slot!=w->lease||
  frame->sequence!=w->lease_sequence||frame->pixels!=w->published[w->lease-1])return 0;
 atomic_store_explicit(&w->slot_state[w->lease-1],FREE,memory_order_release);w->lease=0;w->lease_sequence=0;return 1;
}
