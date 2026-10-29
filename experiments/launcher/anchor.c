#include "anchor.h"
#include <stddef.h>
#include <stdint.h>

#define WORD(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define HEAD 0x8173320cu
#define COUNT 0x81733210u
#define CURRENT 0x81731f70u
#define SYSTEM_STATE 0x81731f8cu
#define EXIT_HOOK 0x81731f88u
#define ORIGINAL_EXIT_HOOK 0x80a12275u
#define TIMER_THREAD 0x8179a5fcu
#define MUTEX_ID 0x4d555445u
#define MAGIC 0x31414c50u
#define RAM_START 0x80000000u
#define RAM_END 0x83000000u

typedef struct {
 uint32_t id,name,recursion,owner,inherit,priority,threshold,wait_first,wait_count,next,previous;
} NativeMutex;
typedef struct {
 NativeMutex mutex;
 uint32_t native_padding[5];
 uint32_t magic,version,bytes,self,ui_thread;
 PeaqResident resident;
 uint32_t padding[3];
} Anchor;
_Static_assert(sizeof(NativeMutex)==44,"Original ThreadX mutex ABI");
_Static_assert(offsetof(Anchor,magic)==64,"Payload outside native mutex");
_Static_assert(sizeof(Anchor)==PEAQ_LAUNCHER_ANCHOR_BYTES,"Permanent allocation");
static const char NAME[]="PEAQ large-game owner v1";
extern uint32_t peaq_launcher_anchor_control(void);
static uint32_t irq(uint32_t mode){return ((uint32_t (*)(uint32_t))0x80acbd7du)(mode);}
static void *allocate(void){
 return ((void *(*)(uint32_t,const char *,uint32_t))0x88090848u)(sizeof(Anchor),NAME,1);
}
static void release(void *p){((void (*)(void *,const char *,uint32_t))0x88090850u)(p,NAME,1);}
static uint32_t create(Anchor *p){
 return ((uint32_t (*)(void *,const char *,uint32_t))0x80bfdff1u)(p,NAME,0);
}
const char *peaq_launcher_anchor_name(void){return NAME;}
static int span(uint32_t p,uint32_t n){return !(p&3u)&&p>=RAM_START&&p<=RAM_END-n;}
static int context(uint32_t t){return t&&t!=TIMER_THREAD&&WORD(CURRENT)==t&&!WORD(SYSTEM_STATE);}

/* IRQ masked; exact count and circular links are checked even after finding
 * the name. Never return a partially traversed or duplicated registry. */
static Anchor *scan(uint32_t task,uint32_t *error){
 uint32_t head=WORD(HEAD),count=WORD(COUNT),node=head;Anchor *found=0;
 *error=PLA_REGISTRY;
 if(WORD(EXIT_HOOK)!=ORIGINAL_EXIT_HOOK)return 0;
 if(count>PEAQ_LAUNCHER_ANCHOR_MAX_MUTEXES){*error=PLA_LIMIT;return 0;}
 if(!count){if(head)return 0;*error=PLA_OK;return 0;}
 for(uint32_t i=0;i<count;i++){
  if(!span(node,sizeof(NativeMutex)))return 0;
  const NativeMutex *m=(const NativeMutex *)(uintptr_t)node;
  if(m->id!=MUTEX_ID||!span(m->next,sizeof(NativeMutex))||!span(m->previous,sizeof(NativeMutex)))return 0;
  const NativeMutex *next=(const NativeMutex *)(uintptr_t)m->next;
  const NativeMutex *previous=(const NativeMutex *)(uintptr_t)m->previous;
  if(next->previous!=node||previous->next!=node)return 0;
  if(m->name==(uint32_t)(uintptr_t)NAME){
   if(found||!span(node,sizeof(Anchor)))return 0;
   Anchor *a=(Anchor *)(uintptr_t)node;
   if(m->recursion||m->owner||m->inherit||m->wait_first||m->wait_count||
      a->magic!=MAGIC||a->version!=1||a->bytes!=sizeof(Anchor)||a->self!=node||a->ui_thread!=task)return 0;
   found=a;
  }
  node=m->next;
  if((node==head)!=(i+1==count))return 0;
 }
 *error=PLA_OK;return found;
}

PeaqResident *peaq_launcher_anchor_owner(uint32_t *out_error){
 uint32_t error=PLA_CONTEXT,control=peaq_launcher_anchor_control(),task=WORD(CURRENT);
 Anchor *result=0,*candidate=0;
 /* Native control's mask operation enables FIQ. Reject an unusual F-masked
  * context before invoking it, rather than momentarily enabling an interrupt. */
 if((control&0x40u)||!context(task))goto done;
 uint32_t saved=irq(0x80u);
 result=scan(task,&error);
 irq(saved);
 if(result||error)goto done;
 /* Never allocate/free in a caller's pre-existing IRQ critical section. */
 if(control&0x80u){error=PLA_CONTEXT;goto done;}
 candidate=allocate();
 if(!candidate){error=PLA_OOM;goto done;}
 for(uint32_t i=0;i<sizeof(*candidate);i++)((uint8_t *)candidate)[i]=0;
 candidate->magic=MAGIC;candidate->version=1;candidate->bytes=sizeof(*candidate);
 candidate->self=(uint32_t)(uintptr_t)candidate;candidate->ui_thread=task;
 saved=irq(0x80u);
 if(!context(task)){error=PLA_CONTEXT;}
 else {
  result=scan(task,&error);
  if(!result&&!error){
   if(WORD(COUNT)==PEAQ_LAUNCHER_ANCHOR_MAX_MUTEXES)error=PLA_LIMIT;
   else if(create(candidate))error=PLA_NATIVE_CREATE;
   else {result=candidate;candidate=0;}
  }
 }
 irq(saved);
 if(candidate)release(candidate);
done:
 if(out_error)*out_error=error;
 return result?&result->resident:0;
}
