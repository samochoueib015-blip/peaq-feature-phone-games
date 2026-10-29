#include "io_channel.h"
#include <stdatomic.h>
#include <stddef.h>

#define CHANNEL_MAGIC 0x3143494cu
typedef struct {
 uint32_t magic,generation,next_sequence;
 _Atomic uint32_t state,closed,done,joined,drained,requests,responses;
 _Atomic uint32_t sequence;uint32_t operation,arg0,arg1,input_bytes,output_limit;
 int32_t value;uint32_t output_bytes;
 uint8_t data[PEAQ_LICAR_IO_PAYLOAD];
} Channel;
_Static_assert(sizeof(Channel)<=PEAQ_LICAR_CHANNEL_BYTES,"Channel storage");
_Static_assert(ATOMIC_INT_LOCK_FREE==2,"Channel requires lock-free 32-bit atomics");
static Channel *channel(PeaqLicarChannel *c){return (Channel *)(void *)c;}
static int valid(const Channel *c){return c&&c->magic==CHANNEL_MAGIC;}
static int matches(const Channel *c,PeaqLicarTicket t){return t.generation==c->generation&&t.sequence&&t.sequence==atomic_load(&c->sequence);}
static void copy(void *to,const void *from,uint32_t n){uint8_t *d=to;const uint8_t *s=from;while(n--)*d++=*s++;}
static void drain(Channel *c){
 if(!atomic_load(&c->closed))return;
 uint32_t s=atomic_load(&c->state);
 while(s==PLIC_REQUEST||s==PLIC_RESPONSE){
  if(atomic_compare_exchange_weak(&c->state,&s,PLIC_IDLE))return;
 }
}
int peaq_licar_channel_init(PeaqLicarChannel *storage,uint32_t generation){
 Channel *c=channel(storage);if(!c||!generation)return 0;
 if(c->magic&&(!valid(c)||generation<=c->generation||!peaq_licar_channel_can_release(storage)))return 0;
 *c=(Channel){0};c->magic=CHANNEL_MAGIC;c->generation=generation;return 1;
}
int peaq_licar_channel_submit(PeaqLicarChannel *storage,uint32_t op,uint32_t arg0,uint32_t arg1,
 const void *input,uint32_t bytes,uint32_t limit,PeaqLicarTicket *ticket){
 Channel *c=channel(storage);
 if(!valid(c)||!ticket||op<PLIC_MODULE_OPEN||op>PLIC_SYNC_CODE||bytes>PEAQ_LICAR_IO_PAYLOAD||limit>PEAQ_LICAR_IO_PAYLOAD||(!input&&bytes))return PLIC_INVALID;
 if(atomic_load(&c->closed)||atomic_load(&c->done))return PLIC_CANCELED;
 if(c->next_sequence==UINT32_MAX){atomic_store(&c->closed,1);return PLIC_EXHAUSTED;}
 uint32_t idle=PLIC_IDLE;if(!atomic_compare_exchange_strong(&c->state,&idle,PLIC_BUILDING))return PLIC_BUSY;
 atomic_store(&c->sequence,++c->next_sequence);c->operation=op;c->arg0=arg0;c->arg1=arg1;
 c->input_bytes=bytes;c->output_limit=limit;copy(c->data,input,bytes);
 *ticket=(PeaqLicarTicket){c->generation,atomic_load(&c->sequence)};atomic_fetch_add(&c->requests,1);
 atomic_store_explicit(&c->state,PLIC_REQUEST,memory_order_release);return PLIC_DONE;
}
int peaq_licar_channel_result(PeaqLicarChannel *storage,PeaqLicarTicket ticket,int32_t *value,
 void *output,uint32_t capacity,uint32_t *bytes){
 Channel *c=channel(storage);if(!valid(c)||!value||!bytes)return PLIC_INVALID;
 if(!matches(c,ticket))return PLIC_STALE;
 if(atomic_load(&c->closed)){drain(c);return PLIC_CANCELED;}
 uint32_t s=atomic_load_explicit(&c->state,memory_order_acquire);
 if(s==PLIC_IDLE)return PLIC_STALE;
 if(s!=PLIC_RESPONSE)return PLIC_PENDING;
 if(c->output_bytes>capacity||(!output&&c->output_bytes))return PLIC_INVALID;
 if(!atomic_compare_exchange_strong(&c->state,&s,PLIC_READING))return PLIC_PENDING;
 if(atomic_load(&c->closed)){atomic_store(&c->state,PLIC_IDLE);return PLIC_CANCELED;}
 copy(output,c->data,c->output_bytes);*value=c->value;*bytes=c->output_bytes;
 int result=atomic_load(&c->closed)?PLIC_CANCELED:PLIC_DONE;
 atomic_store_explicit(&c->state,PLIC_IDLE,memory_order_release);return result;
}
int peaq_licar_channel_take(PeaqLicarChannel *storage,PeaqLicarRequest *out){
 Channel *c=channel(storage);if(!valid(c)||!out)return PLIC_INVALID;
 if(atomic_load(&c->closed)){drain(c);return PLIC_PENDING;}
 uint32_t s=PLIC_REQUEST;if(!atomic_compare_exchange_strong(&c->state,&s,PLIC_UI_OWNED))return PLIC_PENDING;
 if(atomic_load(&c->closed)){atomic_store(&c->state,PLIC_IDLE);return PLIC_PENDING;}
 out->ticket=(PeaqLicarTicket){c->generation,atomic_load(&c->sequence)};out->operation=c->operation;
 out->arg0=c->arg0;out->arg1=c->arg1;out->bytes=c->input_bytes;out->output_limit=c->output_limit;
 copy(out->data,c->data,c->input_bytes);return PLIC_DONE;
}
int peaq_licar_channel_respond(PeaqLicarChannel *storage,PeaqLicarTicket ticket,int32_t value,const void *data,uint32_t bytes){
 Channel *c=channel(storage);if(!valid(c))return PLIC_INVALID;
 if(!matches(c,ticket))return PLIC_STALE;
 uint32_t s=PLIC_UI_OWNED;if(!atomic_compare_exchange_strong(&c->state,&s,PLIC_RESPONDING))return PLIC_STALE;
 if(!matches(c,ticket)){atomic_store(&c->state,PLIC_UI_OWNED);return PLIC_STALE;}
 if(bytes>c->output_limit||(!data&&bytes)){atomic_store(&c->state,PLIC_UI_OWNED);return PLIC_INVALID;}
 if(atomic_load(&c->closed)){atomic_store(&c->state,PLIC_IDLE);return PLIC_CANCELED;}
 c->value=value;c->output_bytes=bytes;copy(c->data,data,bytes);atomic_fetch_add(&c->responses,1);
 if(atomic_load(&c->closed)){atomic_store(&c->state,PLIC_IDLE);return PLIC_CANCELED;}
 atomic_store_explicit(&c->state,PLIC_RESPONSE,memory_order_release);return PLIC_DONE;
}
int peaq_licar_channel_abandon(PeaqLicarChannel *storage,PeaqLicarTicket ticket){
 return peaq_licar_channel_respond(storage,ticket,0,0,0);
}
void peaq_licar_channel_close(PeaqLicarChannel *storage){Channel *c=channel(storage);if(valid(c)){atomic_store(&c->closed,1);drain(c);}}
int peaq_licar_channel_closed(const PeaqLicarChannel *storage){const Channel *c=(const Channel *)(const void *)storage;return !valid(c)||atomic_load(&c->closed);}
void peaq_licar_channel_worker_done(PeaqLicarChannel *storage){Channel *c=channel(storage);if(valid(c)){atomic_store(&c->closed,1);atomic_store_explicit(&c->done,1,memory_order_release);}}
int peaq_licar_channel_join(PeaqLicarChannel *storage,int (*join)(void *,uint32_t),void *cookie,uint32_t handle){
 Channel *c=channel(storage);if(!valid(c)||!join||!handle)return -1;
 if(atomic_load(&c->joined))return 1;if(!atomic_load_explicit(&c->done,memory_order_acquire))return 0;
 int result=join(cookie,handle);if(result==1)atomic_store(&c->joined,1);return result==1?1:result<0?-1:0;
}
void peaq_licar_channel_no_worker(PeaqLicarChannel *storage){Channel *c=channel(storage);if(valid(c)){atomic_store(&c->closed,1);atomic_store(&c->done,1);atomic_store(&c->joined,1);}}
int peaq_licar_channel_idle(PeaqLicarChannel *storage){Channel *c=channel(storage);if(!valid(c))return 0;drain(c);return atomic_load(&c->state)==PLIC_IDLE;}
int peaq_licar_channel_drained(PeaqLicarChannel *storage){Channel *c=channel(storage);if(!valid(c)||!atomic_load(&c->done)||!peaq_licar_channel_idle(storage))return 0;atomic_store(&c->drained,1);return 1;}
int peaq_licar_channel_can_release(PeaqLicarChannel *storage){Channel *c=channel(storage);return valid(c)&&atomic_load(&c->joined)&&atomic_load(&c->drained)&&peaq_licar_channel_idle(storage);}
void peaq_licar_channel_status(const PeaqLicarChannel *storage,PeaqLicarChannelStatus *out){
 const Channel *c=(const Channel *)(const void *)storage;if(!out)return;*out=(PeaqLicarChannelStatus){0};if(!valid(c))return;
 *out=(PeaqLicarChannelStatus){c->generation,atomic_load(&c->sequence),atomic_load(&c->state),atomic_load(&c->closed),
 atomic_load(&c->done),atomic_load(&c->joined),atomic_load(&c->drained),atomic_load(&c->requests),atomic_load(&c->responses)};
}
