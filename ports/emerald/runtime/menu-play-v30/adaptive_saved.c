/* Separate RAM-only trial: dynamic presentation and bounded cached batches.
 * The generated base retains the qualified loader, input and exception paths. */
#include "data_cache.h"
#include "frame_schedule.h"
#include "video_cadence.h"
static int adaptive_target(void *arg);
static int adaptive_reply(void);
static void adaptive_present(void *,const uint16_t *,uint32_t,uint32_t);
static PeaqFrameSchedule schedule;
static unsigned hidden_frame;
#include "adaptive_interactive_base.c"

int peaq_cached_input_context(void){
 uint32_t s=peaq_dc_r_sctlr();
 if(!(s&5u))return 1;
 return s==0x00c5187du&&peaq_dc.active==2u&&peaq_dc.prepared&&!peaq_dc.error&&
  !peaq_dc.leaving&&peaq_dc_r_actlr()==0x6040u&&peaq_dc_r_ttbcr()==0&&
  peaq_dc_r_ttbr0()==PEAQ_DC_TABLE_BASE&&peaq_dc_r_ttbr1()==0&&peaq_dc_r_dacr()==1;
}
static const PeaqVideoStats *video_stats(void){
 if(!runtime.active||runtime.module.base!=runtime_arena.bytes||
    runtime.module.memory_bytes!=PEAQ_ENGINE_MEMORY_BYTES)return 0;
 return (const PeaqVideoStats *)(runtime.module.base+PEAQ_ENGINE_VIDEO_RVA);
}
static int video_valid(void){
 const PeaqVideoStats *v=video_stats();
 return v&&v->magic==PEAQ_VIDEO_MAGIC&&v->version==1&&v->bytes==64&&!v->error&&
  v->frames==runtime.frames&&v->frames==intro.frames&&v->shown==schedule.drawn&&
  v->hidden==schedule.hidden&&v->shown+v->hidden==v->frames&&
  v->dependent+v->independent==v->frames&&v->ppu_calls<=v->frames&&
  v->pending<=1&&v->shown==intro.present_count;
}
static void adaptive_present(void *cookie,const uint16_t *p,uint32_t w,uint32_t h){
 /* Hidden frames retain the callback contract and input polling; only DMA
  * is omitted. Reject bad arguments even when no display transfer is due. */
 if(cookie!=&storage.fs||p!=runtime.portrait||w!=240u||h!=320u){
  intro.callback_fault=INTRO_PRESENT;return;
 }
 if(!hidden_frame)present(cookie,p,w,h);
}
static int adaptive_reply(void){
 struct {uint32_t magic,version,bytes,frames;PeaqFrameSchedule clock;PeaqVideoStats video;} r;
 _Static_assert(sizeof(r)==128,"EVR1 wire layout");
 memset(&r,0,sizeof(r));r.magic=0x31525645u;r.version=1;r.bytes=sizeof(r);r.frames=intro.frames;
 memcpy(&r.clock,&schedule,sizeof(schedule));
 const PeaqVideoStats *v=video_stats();if(v)memcpy(&r.video,v,sizeof(*v));
 return checked_reply(0xa4,&r,sizeof(r));
}
static int adaptive_target(void *arg){
 if(arg!=&runtime||!peaq_dc.prepared)return cache_fail(CACHE_CONTEXT);
 cache.cpsr_before=peaq_fault.cpsr_before;cache.armed_sctlr=peaq_fault.armed_sctlr;
 if(peaq_cache_read_sctlr()!=peaq_fault.armed_sctlr||
    peaq_cache_read_actlr()!=expected_actlr()||!(cache.flags&CACHE_F_SMP))
  return cache_fail(CACHE_CONTEXT);
 uint32_t t=sys_timer_ms();++cache.enable_attempts;
 if(!peaq_dc_enter(1))return cache_fail(CACHE_READBACK);
 cache.enabled_sctlr=peaq_dc.observed_enabled;cache.flags|=CACHE_F_ICACHE_VERIFIED;
 cache.transition_ms+=sys_timer_ms()-t;
 ++cache.target_entered;peaq_fault.breadcrumb=20;
 /* Start the clock after the first cache entry, not during engine loading. */
 if(!schedule.initialized)peaq_frame_schedule_init(&schedule,sys_timer_ms());
 unsigned count=intro.frames?256u:1u;
 for(unsigned i=0;i<count&&intro.frames<PEAQ_INTERACTIVE_FRAME_LIMIT;++i){
  uint32_t now=sys_timer_ms(),last=now;unsigned stalled=0;
  int decision;
  for(;;){
   service_input(now,1);
   if(interactive.stop_requested||menu_pause_pending)goto done;
   decision=peaq_frame_schedule_next(&schedule,now);
   if(decision!=PEAQ_FRAME_WAIT)break;
   now=sys_timer_ms();
   if(now==last){if(++stalled>=1000000u){
    interactive_error(INTERACTIVE_TIMER,INTERACTIVE_STOP_TIMER);goto done;
   }}else stalled=0;
   last=now;
  }
  if(decision!=PEAQ_FRAME_DRAW&&decision!=PEAQ_FRAME_HIDDEN)return cache_fail(CACHE_ENGINE);
  if(!peaq_interactive_keys_take(&key_frame)){
   interactive_error(INTERACTIVE_KEY_READ,INTERACTIVE_STOP_INPUT);break;
  }
  if((key_frame.host|key_frame.host_pressed)&PEAQ_KEY_HOST_BACK){
   menu_request_exit();break;
  }
  if(!all_guards()||!outer_result()||!peaq_interactive_keys_guards())return cache_fail(CACHE_GUARD);
  hidden_frame=decision==PEAQ_FRAME_HIDDEN;
  /* 17,17,16 ms: exactly one existing runtime step per 60-Hz deadline. */
  logical_ms=((intro.frames+1u)*1000u+59u)/60u;
  uint32_t begin=sys_timer_ms();
  int result=peaq_emerald_runtime_step(&runtime,logical_ms,
    key_frame.emerald_buttons|(hidden_frame?PEAQ_EMERALD_STEP_HIDDEN:0));
  intro.step_ms+=sys_timer_ms()-begin;intro.runtime_result=(uint32_t)result;
  if(result!=1)return cache_fail(CACHE_ENGINE);
  ++intro.frames;
  if(!outer_result()||!video_valid())return cache_fail(CACHE_ENGINE);
  if(runtime.dirty&&runtime.quiet_frames>=30)break;
 }
done:
 peaq_fault.breadcrumb=40;++cache.target_returned;return 1;
}
