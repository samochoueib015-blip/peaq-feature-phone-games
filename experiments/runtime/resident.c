#include "resident.h"
#include <stddef.h>

#define MAGIC 0x31475250u
enum { IDLE,DISPATCH,CLOSE,POLL,RELEASE };
typedef struct {
 uint32_t magic,generation,state,busy,sequence,ready,close_sent;
 int32_t last_poll_error;
 PeaqResidentOps ops;
} Owner;
_Static_assert(sizeof(Owner)<=PEAQ_RESIDENT_BYTES,"Resident owner storage");
static Owner *owner(PeaqResident *p){return (Owner *)(void *)p;}
static int valid(const Owner *p){return p&&p->magic==MAGIC;}
static int occupied(const Owner *p){return valid(p)&&(p->state==PRG_ATTACHED||p->state==PRG_CLOSING);}
static int zero(const PeaqResident *p){
 if(!p)return 0;
 for(uint32_t i=0;i<PEAQ_RESIDENT_BYTES;i++)if(p->bytes[i])return 0;
 return 1;
}
static void cancel(Owner *p){
 if(p->state!=PRG_CLOSING||p->busy||p->close_sent)return;
 p->close_sent=1;p->busy=CLOSE;p->ops.close(p->ops.cookie);p->busy=IDLE;
}
uint32_t peaq_resident_claim(PeaqResident *storage,const PeaqResidentOps *ops){
 Owner *p=owner(storage);
 if(!p||!ops||!ops->cookie||!ops->close||!ops->poll||!ops->release)return 0;
 PeaqResidentOps saved=*ops;
 if(!p->magic){
  if(!zero(storage))return 0;
  p->magic=MAGIC;
 }
 if(!valid(p)||p->state!=PRG_EMPTY||p->busy||p->generation==UINT32_MAX)return 0;
 p->generation++;p->sequence=p->ready=p->close_sent=0;p->last_poll_error=0;
 p->ops=saved;p->state=PRG_ATTACHED;return p->generation;
}
int peaq_resident_enter(PeaqResident *storage,uint32_t generation,PeaqResidentUse *use){
 Owner *p=owner(storage);
 if(!use||!valid(p)||p->generation!=generation||p->state!=PRG_ATTACHED||p->busy||p->ready)return 0;
 /* Exhaustion rejects dispatch, but does not detach a view behind its back. */
 if(p->sequence==UINT32_MAX)return 0;
 p->sequence++;p->busy=DISPATCH;
 *use=(PeaqResidentUse){generation,p->sequence,p->ops.cookie};return 1;
}
int peaq_resident_leave(PeaqResident *storage,const PeaqResidentUse *use){
 Owner *p=owner(storage);
 if(!use||!occupied(p)||p->busy!=DISPATCH||p->generation!=use->generation||
    p->sequence!=use->sequence||p->ops.cookie!=use->cookie)return 0;
 p->busy=IDLE;cancel(p);return 1;
}
int peaq_resident_close(PeaqResident *storage,uint32_t generation){
 Owner *p=owner(storage);if(!occupied(p)||p->generation!=generation)return 0;
 p->state=PRG_CLOSING;cancel(p);return 1;
}
int peaq_resident_poll(PeaqResident *storage,uint32_t milliseconds){
 Owner *p=owner(storage);if(!valid(p))return zero(storage)?PRG_REAPED:PRG_INVALID;
 if(p->busy)return PRG_PENDING;
 if(p->state==PRG_EMPTY)return PRG_REAPED;
 if(!occupied(p))return PRG_INVALID;
 cancel(p);
 if(!p->ready){
  p->busy=POLL;int result=p->ops.poll(p->ops.cookie,milliseconds);p->busy=IDLE;
  if(result==1){p->ready=1;p->last_poll_error=0;}
  else p->last_poll_error=result==0?0:result;
  /* A forced window close may have reentered during a native UI operation. */
  cancel(p);
 }
 if(!p->ready)return p->last_poll_error?PRG_ERROR:PRG_PENDING;
 if(p->state==PRG_ATTACHED)return PRG_COMPLETE_ATTACHED;
 p->state=PRG_RELEASING;p->busy=RELEASE;
 p->ops.release(p->ops.cookie);
 /* Do not dereference any owned pointer after the release callback. */
 p->ops=(PeaqResidentOps){0};p->ready=p->close_sent=0;p->last_poll_error=0;
 p->state=PRG_EMPTY;p->busy=IDLE;return PRG_REAPED;
}
void peaq_resident_status(const PeaqResident *storage,PeaqResidentStatus *out){
 const Owner *p=(const Owner *)(const void *)storage;
 if(!out)return;*out=(PeaqResidentStatus){0};if(!valid(p))return;
 *out=(PeaqResidentStatus){p->generation,p->state,p->busy,p->ready,p->close_sent,p->sequence,p->last_poll_error};
}
