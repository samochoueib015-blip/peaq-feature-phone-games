#include "native_controls.h"
#include "runtime.h"
#include "platform.h"
#include "../runtime/module.h"
#include <stdatomic.h>
#include <stddef.h>

#define MAGIC 0x31524c50u
#define PIXELS (240u*320u)
#define FRAME_BYTES (PIXELS*2u)
enum { FREE, WRITING, READY, READING };
typedef struct {
 uint32_t magic;
 PeaqLicarServices io;
 _Atomic uint32_t state,error,stop,paused,held[2],edges[2];
 _Atomic uint32_t frames,yields,steps,buttons,engine_state,speed,audio_samples,audio_dropped,engine_flags;
 uint32_t thread,joined,lease,lease_sequence;
 PeaqModule module;
 void *frame_allocation;
 uint16_t *land,*portrait,*published[2];
 _Atomic uint32_t slot_state[2];
 uint32_t slot_sequence[2];
 LicarHost host;
} Run;
_Static_assert(sizeof(Run)<=PEAQ_LICAR_RUNTIME_BYTES,"Runtime storage too small");
_Static_assert(ATOMIC_INT_LOCK_FREE==2,"32-bit atomics must be lock-free");
/* The existing engine callback ABI has no cookie. Own this bridge only on
 * one worker; another queued instance fails without executing a module. */
static _Atomic(Run *) bridge;
static Run *run(PeaqLicarRuntime *r){return (Run *)(void *)r;}
static int valid(const Run *r){return r&&r->magic==MAGIC;}
static void error(Run *r,uint32_t e){uint32_t zero=0;atomic_compare_exchange_strong(&r->error,&zero,e);}
static void clear_keys(Run *r){for(unsigned i=0;i<2;i++){atomic_store(&r->held[i],0);atomic_store(&r->edges[i],0);}}

static int stream_read(void *cookie,void *p,uint32_t n,uint32_t *got){
 Run *r=cookie;*got=0;
 if(atomic_load(&r->stop))return 0;
 int ok=r->io.module_read(r->io.cookie,p,n,got);
 if(ok!=1||*got>n){*got=0;return 0;}
 /* Relocation/checksum work also runs on the worker; allow other OS tasks
  * to run while reading a large module. */
 r->io.sleep_ms(r->io.cookie,1);
 return !atomic_load(&r->stop);
}
static void *module_alloc(uint32_t n){
 Run *r=atomic_load(&bridge);
 return n<=1024u*1024u+31u?r->io.allocate(r->io.cookie,n):0;
}
static void module_free(void *p){Run *r=atomic_load(&bridge);r->io.release(r->io.cookie,p);}
static void module_sync(void *p,uint32_t n){Run *r=atomic_load(&bridge);r->io.sync_code(r->io.cookie,p,n);}
static void present(const uint16_t *p,uint32_t width,uint32_t height){
 Run *r=atomic_load(&bridge);
 if(p!=r->portrait||width!=240||height!=320){error(r,PLR_FRAME);atomic_store(&r->stop,1);return;}
 if(atomic_load(&r->stop)||atomic_load(&r->paused))return;
 for(unsigned i=0;i<2;i++){
  uint32_t status=atomic_load(&r->slot_state[i]);
  if(status!=FREE&&status!=READY)continue;
  if(!atomic_compare_exchange_strong(&r->slot_state[i],&status,WRITING))continue;
  for(uint32_t j=0;j<PIXELS;j++)r->published[i][j]=p[j];
  uint32_t sequence=atomic_fetch_add(&r->frames,1)+1;
  r->slot_sequence[i]=sequence;
  atomic_store_explicit(&r->slot_state[i],READY,memory_order_release);return;
 }
}
static void yield(void){
 Run *r=atomic_load(&bridge);atomic_fetch_add(&r->yields,1);
 /* A live engine call may still hold pointers on its stack. Cancellation is
  * observed after that call returns; never unwind/free it from this callback. */
 do{r->io.sleep_ms(r->io.cookie,1);}while(atomic_load(&r->paused)&&!atomic_load(&r->stop));
}
static void log_message(const char *s){(void)s;}
static char data_next(void){Run *r=atomic_load(&bridge);return r->io.data_next?r->io.data_next(r->io.cookie):0;}
static int data_append(const char *s){Run *r=atomic_load(&bridge);return r->io.data_append?r->io.data_append(r->io.cookie,s):0;}
static uint32_t music_read(uint8_t *p,uint32_t n){Run *r=atomic_load(&bridge);return r->io.music_read?r->io.music_read(r->io.cookie,p,n):0;}
static uint32_t sample(Run *r){
 uint32_t a=atomic_load(&r->held[0])|atomic_exchange(&r->edges[0],0);
 uint32_t b=atomic_load(&r->held[1])|atomic_exchange(&r->edges[1],0);
 return peaq_licar_control_actions(a,b,atomic_load(&r->engine_state));
}
static void worker_entry(void *cookie){
 Run *r=cookie,*empty=0;
 if(!atomic_compare_exchange_strong(&bridge,&empty,r)){
  error(r,PLR_BUSY);r->io.finish(r->io.cookie);
  atomic_store(&r->state,PLR_FINISHED);return;
 }
 atomic_store(&r->state,PLR_LOADING);
 int initialized=0,opened=0;
 if(atomic_load(&r->stop))goto done;
 if(r->io.module_open(r->io.cookie)!=1){error(r,PLR_MODULE_OPEN);goto done;}
 opened=1;
 PeaqModuleHost loader={r,stream_read,module_alloc,module_free,module_sync};
 int result=peaq_module_load(&loader,&r->module);
 if(r->io.module_close(r->io.cookie)!=1){error(r,PLR_MODULE_CLOSE);opened=0;goto done;}
 opened=0;
 if(atomic_load(&r->stop))goto done;
 if(result!=1||r->module.export_count!=8){error(r,PLR_MODULE);goto done;}
 r->frame_allocation=r->io.allocate(r->io.cookie,4u*FRAME_BYTES+31u);
 if(!r->frame_allocation){error(r,PLR_MEMORY);goto done;}
 uintptr_t base=((uintptr_t)r->frame_allocation+31u)&~(uintptr_t)31u;
 r->land=(uint16_t *)base;r->portrait=r->land+PIXELS;
 r->published[0]=r->portrait+PIXELS;r->published[1]=r->published[0]+PIXELS;
 r->host=(LicarHost){present,yield,log_message,data_next,data_append,music_read};
 int (*init)(const LicarHost *,uint16_t *,uint16_t *)=(void *)(uintptr_t)r->module.exports[0];
 if(init(&r->host,r->land,r->portrait)!=1){error(r,PLR_ENGINE_INIT);goto done;}
 initialized=1;
 if(atomic_load(&r->stop))goto done;
 atomic_store(&r->state,PLR_RUNNING);
 uint32_t last=r->io.milliseconds(r->io.cookie);
 while(!atomic_load(&r->stop)){
  if(atomic_load(&r->paused)){r->io.sleep_ms(r->io.cookie,20);last=r->io.milliseconds(r->io.cookie);continue;}
  uint32_t now=r->io.milliseconds(r->io.cookie),elapsed=now-last;
  if(elapsed<50){r->io.sleep_ms(r->io.cookie,50-elapsed);continue;}
  last=now;if(elapsed>100)elapsed=100;
  uint32_t buttons=sample(r);atomic_store(&r->buttons,buttons);
  int (*step)(uint32_t,uint32_t)=(void *)(uintptr_t)r->module.exports[1];
  result=step(elapsed,buttons);atomic_fetch_add(&r->steps,1);
  uint32_t (*state)(void)=(void *)(uintptr_t)r->module.exports[4];
  int32_t (*speed)(void)=(void *)(uintptr_t)r->module.exports[5];
  atomic_store(&r->engine_state,state());atomic_store(&r->speed,(uint32_t)speed());
  if(!result||atomic_load(&r->stop))break;
  if(r->io.audio_submit&&!atomic_load(&r->paused)){
   int16_t pcm[800];uint32_t samples=elapsed*8u;
   void (*audio)(int16_t *,uint32_t)=(void *)(uintptr_t)r->module.exports[2];
   audio(pcm,samples);uint32_t accepted=r->io.audio_submit(r->io.cookie,pcm,samples);
   if(accepted>samples){error(r,PLR_AUDIO);break;}
   atomic_fetch_add(&r->audio_samples,accepted);atomic_fetch_add(&r->audio_dropped,samples-accepted);
  }
 }
done:
 if(opened)r->io.module_close(r->io.cookie);
 if(initialized){
  void (*end)(void)=(void *)(uintptr_t)r->module.exports[3];end();
  uint32_t (*flags)(void)=(void *)(uintptr_t)r->module.exports[7];atomic_store(&r->engine_flags,flags());
 }
 r->io.finish(r->io.cookie);
 atomic_store(&bridge,0);
 /* This flag alone is insufficient for unloading: poll must observe a real
  * completed thread join before releasing memory or the parent PGA code. */
 atomic_store(&r->state,PLR_FINISHED);
}
int peaq_licar_runtime_start(PeaqLicarRuntime *storage,const PeaqLicarServices *io){
 Run *r=run(storage);
 if(!r||!io||!io->start||!io->join||!io->milliseconds||!io->sleep_ms||!io->module_open||
    !io->module_read||!io->module_close||!io->allocate||!io->release||!io->sync_code||!io->finish)return 0;
 if(r->magic&&(r->magic!=MAGIC||atomic_load(&r->state)!=PLR_REAPED))return 0;
 *r=(Run){0};r->magic=MAGIC;r->io=*io;atomic_store(&r->state,PLR_QUEUED);
 r->thread=io->start(io->cookie,worker_entry,r,PEAQ_LICAR_WORKER_STACK);
 if(!r->thread){error(r,PLR_THREAD);atomic_store(&r->state,PLR_REAPED);return 0;}
 return 1;
}
void peaq_licar_runtime_input(PeaqLicarRuntime *storage,uint32_t value){
 Run *r=run(storage);if(!valid(r)||atomic_load(&r->paused)||atomic_load(&r->stop))return;
 uint32_t key=value&255,kind=value&0xff00;
 if(!((key>=1&&key<=4)||(key>=8&&key<=0x13)||key==0x16||key==0x17||key==0x1b||key==0x26))return;
 uint32_t word=key/32,bit=1u<<(key%32);
 if(kind==0xfa00){uint32_t old=atomic_fetch_or(&r->held[word],bit);if(!(old&bit))atomic_fetch_or(&r->edges[word],bit);}
 else if(kind==0xfb00||kind==0xfd00)atomic_fetch_and(&r->held[word],~bit);
}
void peaq_licar_runtime_pause(PeaqLicarRuntime *storage,int pause){
 Run *r=run(storage);if(!valid(r))return;
 atomic_store(&r->paused,!!pause);clear_keys(r);
}
void peaq_licar_runtime_stop(PeaqLicarRuntime *storage){Run *r=run(storage);if(valid(r)){atomic_store(&r->stop,1);clear_keys(r);}}
int peaq_licar_runtime_poll(PeaqLicarRuntime *storage){
 Run *r=run(storage);if(!valid(r))return 0;
 uint32_t state=atomic_load(&r->state);if(state==PLR_REAPED)return 1;if(state!=PLR_FINISHED)return 0;
 if(!r->joined){int joined=r->io.join(r->io.cookie,r->thread);if(joined!=1){if(joined<0)error(r,PLR_JOIN);return 0;}r->joined=1;r->thread=0;}
 if(r->lease)return 0;
 if(r->frame_allocation){r->io.release(r->io.cookie,r->frame_allocation);r->frame_allocation=0;}
 if(r->module.allocation){r->io.release(r->io.cookie,r->module.allocation);r->module=(PeaqModule){0};}
 r->land=r->portrait=r->published[0]=r->published[1]=0;
 atomic_store(&r->state,PLR_REAPED);return 1;
}
void peaq_licar_runtime_status(const PeaqLicarRuntime *storage,PeaqLicarStatus *out){
 const Run *r=(const Run *)(const void *)storage;if(!out)return;*out=(PeaqLicarStatus){0};if(!valid(r))return;
 *out=(PeaqLicarStatus){atomic_load(&r->state),atomic_load(&r->error),atomic_load(&r->stop),atomic_load(&r->paused),
  atomic_load(&r->frames),atomic_load(&r->yields),atomic_load(&r->steps),atomic_load(&r->buttons),
  atomic_load(&r->engine_state),(int32_t)atomic_load(&r->speed),atomic_load(&r->audio_samples),atomic_load(&r->audio_dropped),atomic_load(&r->engine_flags)};
}
int peaq_licar_runtime_frame_acquire(PeaqLicarRuntime *storage,PeaqLicarFrame *out){
 Run *r=run(storage);if(!out)return 0;*out=(PeaqLicarFrame){0};
 if(!valid(r)||r->lease)return 0;uint32_t phase=atomic_load(&r->state);if(phase!=PLR_RUNNING&&phase!=PLR_FINISHED)return 0;
 for(unsigned i=0;i<2;i++){
  uint32_t ready=READY;if(!atomic_compare_exchange_strong_explicit(&r->slot_state[i],&ready,READING,memory_order_acquire,memory_order_relaxed))continue;
  r->lease=i+1;r->lease_sequence=r->slot_sequence[i];*out=(PeaqLicarFrame){r->published[i],i+1,r->lease_sequence};return 1;
 }
 return 0;
}
int peaq_licar_runtime_frame_release(PeaqLicarRuntime *storage,const PeaqLicarFrame *frame){
 Run *r=run(storage);if(!valid(r)||!frame||!r->lease||frame->slot!=r->lease||frame->sequence!=r->lease_sequence||
                          frame->pixels!=r->published[r->lease-1])return 0;
 atomic_store_explicit(&r->slot_state[r->lease-1],FREE,memory_order_release);r->lease=0;r->lease_sequence=0;return 1;
}
