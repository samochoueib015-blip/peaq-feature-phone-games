/* Separate fixed 240-frame trial: retain data cache across <=16 frames.
 * The generated base preserves the already-qualified engine/SD/display path.
 * No input, audio, storage writes, clock changes or OS handoff. */
#include "data_cache.h"
static int cached_prepare(void);
static int cached_frames_target(void *arg);
static int translation_restored(void);
void peaq_cache_fault_leave(void);
#include "cached_intro_base.c"

static int translation_restored(void){
 return !peaq_dc.snapshot||(peaq_dc.restored&&!peaq_dc.active&&!peaq_dc.leaving&&
  peaq_dc_r_ttbcr()==peaq_dc.ttbcr&&peaq_dc_r_ttbr0()==peaq_dc.ttbr0&&
  peaq_dc_r_ttbr1()==peaq_dc.ttbr1&&peaq_dc_r_dacr()==peaq_dc.dacr&&
  peaq_dc_r_csselr()==peaq_dc.csselr);
}
void peaq_cache_fault_leave(void){
 uint32_t t=sys_timer_ms(),was=peaq_dc_r_sctlr();
 if(!peaq_dc_leave())hard_fail(CACHE_STATE);
 cache.disabled_sctlr=peaq_dc_r_sctlr();
 if(cache.disabled_sctlr!=peaq_fault.armed_sctlr||!translation_restored())hard_fail(CACHE_READBACK);
 if(was&0x1000u)++cache.disable_attempts;
 cache.transition_ms+=sys_timer_ms()-t;
}
static int prepare_target(void *arg){return arg==&runtime&&peaq_dc_prepare();}
static int cached_prepare(void){
 if(peaq_dc.snapshot||peaq_dc.prepared)return cache_fail(CACHE_REPEAT);
 prepare_fault();
 int result=peaq_fault_call(prepare_target,&runtime);
 if(!finish_fault(result)||peaq_dc.error||!peaq_dc.prepared)return cache_fail(CACHE_STATE);
 return 1;
}
static int cached_frames_target(void *arg){
 if(arg!=&runtime||limit<1||limit>16||!peaq_dc.prepared)return cache_fail(CACHE_CONTEXT);
 cache.cpsr_before=peaq_fault.cpsr_before;cache.armed_sctlr=peaq_fault.armed_sctlr;
 if(peaq_cache_read_sctlr()!=peaq_fault.armed_sctlr||peaq_cache_read_actlr()!=expected_actlr()||
    !(cache.flags&CACHE_F_SMP))return cache_fail(CACHE_CONTEXT);
 uint32_t t=sys_timer_ms();++cache.enable_attempts;
 if(!peaq_dc_enter(1))return cache_fail(CACHE_READBACK);
 cache.enabled_sctlr=peaq_dc.observed_enabled;cache.flags|=CACHE_F_ICACHE_VERIFIED;
 cache.transition_ms+=sys_timer_ms()-t;
 ++cache.target_entered;peaq_fault.breadcrumb=20;
 for(unsigned i=0;i<limit&&intro.frames<240u;++i){
  if(!outer_result())return cache_fail(CACHE_ENGINE);
  uint32_t n=intro.frames,buttons=(n==180u||n==181u)?8u:0u;
  logical_ms+=17u;uint32_t begin=sys_timer_ms();
  int result=peaq_emerald_runtime_step(&runtime,logical_ms,buttons);
  intro.step_ms+=sys_timer_ms()-begin;intro.runtime_result=(uint32_t)result;
  if(result!=1)return cache_fail(CACHE_ENGINE);
  ++intro.frames;
  if(!outer_result())return cache_fail(CACHE_ENGINE);
 }
 peaq_fault.breadcrumb=40;++cache.target_returned;return 1;
}
