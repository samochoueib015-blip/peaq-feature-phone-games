/* New fixed-cache, one-frame interactive RAM diagnostic. Frozen cache/intro
 * implementations supply their loader and CPU boundary, not their old loop. */
#include "cached_intro.c"
#include "interactive_diag.h"
#include "interactive_keys.h"

static PeaqInteractiveReport interactive;
static uint32_t in_engine,last_service_ms,last_clock,clock_seen,idle_since;
static PeaqKeyFrame key_frame;
_Static_assert(sizeof(PeaqInteractiveReport)==240,"EIR1 layout");

static void interactive_snapshot(void){
 if(!interactive.stopped){
  const PeaqInteractiveKeys *k=peaq_interactive_keys_status();
  const PeaqKeyInput *p=peaq_interactive_keys_input();
  const PeaqFrameReport *d=peaq_cold_display_status();
  interactive.frames=intro.frames;interactive.logical_ms=logical_ms;
  if(interactive.active)interactive.session_ms=sys_timer_ms()-interactive.start_ms;
  interactive.input_polls=k->polls;interactive.input_records=k->records;
  interactive.input_ring_overwrites=k->overwrites;interactive.input_max_gap_ms=k->max_gap_ms;
  interactive.held0=p->held[0];interactive.held1=p->held[1];interactive.gba=p->gba;interactive.host=p->host;
  interactive.input_presses=p->press_count;interactive.input_releases=p->release_count;
  interactive.input_longs=p->long_count;interactive.input_error=k->error;
  interactive.quiet_ms=k->quiet_ms;interactive.quiet_ready=k->quiet_ready;
  if(k->quiet_ready)interactive.flags|=INTERACTIVE_F_QUIET;else interactive.flags&=~INTERACTIVE_F_QUIET;
  interactive.save_dirty=runtime.dirty;interactive.save_generation=runtime.generation;
  interactive.write_attempts=intro.write_attempts;interactive.runtime_error=(uint32_t)runtime.last_error;
  interactive.cache_smp=!!(cache.flags&CACHE_F_SMP);
  interactive.cache_enabled_count=cache.enable_attempts;interactive.cache_disabled_count=cache.disable_attempts;
  interactive.cache_faults=cache.cpu_faults;interactive.cpu_restored=peaq_fault.control_restored;
  interactive.guards_ok=guards()&&fault_guards()&&(!k->initialized||peaq_interactive_keys_guards());
  interactive.display_frames=d->frames_done;interactive.display_error=d->error;interactive.sd_error=sd.error;
  interactive.awaiting_power_cycle=cache.awaiting_power_cycle;
  interactive.sd_write_commands=peaq_sd_save_status()->writes;
 }
 interactive.current_sctlr=peaq_cache_read_sctlr();interactive.current_actlr=peaq_cache_read_actlr();
}
static void request_stop(unsigned reason){
 if(!interactive.stop_requested){interactive.stop_requested=1;interactive.stop_reason=reason;}
}
static void interactive_error(unsigned error,unsigned reason){
 if(!interactive.error){interactive.error=error;interactive.stop_reason=reason;}
 interactive.stop_requested=1;interactive.flags|=INTERACTIVE_F_FAILED;
}
static void finish_stop(void){
 if(!interactive.stop_requested||interactive.stopped||in_engine)return;
 /* A transport wait never starts SD I/O; finish on the next dispatch. */
 if(saved_waiting&&runtime.dirty&&!interactive.error)return;
 usb_gate();
 peaq_emerald_runtime_pause(&runtime,1,logical_ms);
 if(!interactive.error&&!saved_flush(1))interactive_error(INTERACTIVE_SAVE,INTERACTIVE_STOP_SAVE);
 peaq_interactive_keys_stop();
 if(peaq_interactive_keys_status()->error&&!interactive.error)
  interactive_error(INTERACTIVE_KEY_READ,INTERACTIVE_STOP_INPUT);
 interactive_snapshot();
 interactive.active=0;interactive.stopped=interactive.terminal=1;
 interactive.stage=interactive.error?INTERACTIVE_FAILED:INTERACTIVE_STOPPED;
 interactive.flags=(interactive.flags&~INTERACTIVE_F_RUNNING)|INTERACTIVE_F_STOPPED;
 cache.terminal=1;
}
static void service_input(uint32_t now,int force){
 if(interactive.stopped)return;
 if(clock_seen&&((now-last_clock)&0x80000000u))interactive_error(INTERACTIVE_TIMER,INTERACTIVE_STOP_TIMER);
 clock_seen=1;last_clock=now;
 const PeaqInteractiveKeys *k=peaq_interactive_keys_status();
 if(k->initialized&&!k->stopped&&(force||now!=last_service_ms)){
  last_service_ms=now;
  if(!peaq_interactive_keys_poll(now))interactive_error(INTERACTIVE_KEY_READ,INTERACTIVE_STOP_INPUT);
  else {saved_volume(now);if(saved.error)interactive_error(INTERACTIVE_KEY_READ,INTERACTIVE_STOP_INPUT);}
 }
 if(interactive.active){
  const PeaqKeyInput *p=peaq_interactive_keys_input();
  if((p->host|p->host_pressed)&PEAQ_KEY_HOST_BACK)request_stop(INTERACTIVE_STOP_BACK);
  if(now-interactive.start_ms>=PEAQ_INTERACTIVE_SESSION_MS)request_stop(INTERACTIVE_STOP_DEADLINE);
  /* Host cannot send the next STEP while this synchronous frame executes.
   * Only time available to the host counts toward the idle deadline. */
  if(!in_engine&&now-idle_since>=PEAQ_INTERACTIVE_HOST_IDLE_MS)request_stop(INTERACTIVE_STOP_HOST_IDLE);
 }
}
void peaq_interactive_wait_hook(void){
 if(interactive.magic!=PEAQ_INTERACTIVE_MAGIC||in_engine)return;
 saved_waiting=1;service_input(sys_timer_ms(),0);finish_stop();saved_waiting=0;
}
void peaq_interactive_transport_failure(void){
 interactive_error(INTERACTIVE_TRANSPORT,INTERACTIVE_STOP_TRANSPORT);
 finish_stop();
 /* A USB descriptor can still own packet storage. Do not reuse/free it to
  * attempt a second transfer or claim a final reply after uncertain DMA. */
 peaq_cache_halt();
}
static void interactive_present(void *cookie,const uint16_t *pixels,uint32_t w,uint32_t h){
 service_input(sys_timer_ms(),1);
 if(!interactive.error)adaptive_present(cookie,pixels,w,h);
 service_input(sys_timer_ms(),1);
}
static void step(void){
 if(!interactive.active){interactive_error(INTERACTIVE_ORDER,INTERACTIVE_STOP_ENGINE);return;}
 if(++interactive.step_requests>PEAQ_INTERACTIVE_REQUEST_LIMIT){interactive_error(INTERACTIVE_REQUEST_BUDGET,INTERACTIVE_STOP_REQUEST_LIMIT);return;}
 uint32_t now=sys_timer_ms();service_input(now,1);
 if(intro.frames>=PEAQ_INTERACTIVE_FRAME_LIMIT)request_stop(INTERACTIVE_STOP_FRAME_LIMIT);
 if(interactive.stop_requested)return;
 if(!all_guards()||!outer_result()||!peaq_interactive_keys_guards()){
  interactive_error(INTERACTIVE_GUARD,INTERACTIVE_STOP_ENGINE);return;
 }
 usb_gate();prepare_fault();++cache.batch_count;++intro.batch_count;
 cache.stage=CACHE_RUNNING;intro.stage=INTRO_RUNNING;
 uint32_t before=intro.frames,t=sys_timer_ms();in_engine=1;
 int completed=peaq_fault_call(adaptive_target,&runtime);
 in_engine=0;idle_since=sys_timer_ms();
 cache.last_batch_ms=sys_timer_ms()-t;cache.total_batch_ms+=cache.last_batch_ms;
 peaq_fault.step_ms=cache.last_batch_ms;cache.last_batch_frames=intro.frames-before;
 intro.last_batch_ms=cache.last_batch_ms;intro.wall_ms=sys_timer_ms()-started_ms;
 interactive.step_last_ms=cache.last_batch_ms;interactive.step_total_ms+=cache.last_batch_ms;
 if(!finish_fault(completed))interactive_error(cache.cpu_faults?INTERACTIVE_CPU:INTERACTIVE_ENGINE,
                                             cache.cpu_faults?INTERACTIVE_STOP_CPU:INTERACTIVE_STOP_ENGINE);
 if(interactive.error||intro.write_attempts){
  if(intro.write_attempts)interactive_error(INTERACTIVE_SAVE,INTERACTIVE_STOP_SAVE);
 }else {
  cache.stage=CACHE_RELEASED;
  if(!saved_flush(0))interactive_error(INTERACTIVE_SAVE,INTERACTIVE_STOP_SAVE);
 }
 service_input(sys_timer_ms(),1);
 if(intro.frames==PEAQ_INTERACTIVE_FRAME_LIMIT){
  intro.flags|=INTRO_F_COMPLETE;intro.stage=INTRO_COMPLETE;
  cache.flags|=CACHE_F_COMPLETE;cache.stage=CACHE_COMPLETE;
  request_stop(INTERACTIVE_STOP_FRAME_LIMIT);
 }
}
static void quiet_window(void){
 const PeaqInteractiveKeys *k=peaq_interactive_keys_status();
 if(!k->initialized||interactive.active){interactive_error(INTERACTIVE_ORDER,INTERACTIVE_STOP_INPUT);return;}
 uint32_t start=sys_timer_ms(),prev=start,stalled=0;
 for(unsigned i=0;i<64000000u;++i){
  uint32_t now=sys_timer_ms();service_input(now,1);
  if(interactive.stop_requested)return;
  if(now-start>=PEAQ_INTERACTIVE_QUIET_MS){
   if(k->quiet_ready&&interactive.stage==INTERACTIVE_KEYS_READY)interactive.stage=INTERACTIVE_QUIET_READY;
   return;
  }
  if(now==prev){if(++stalled==1000000u)break;}else stalled=0;
  prev=now;
 }
 interactive_error(INTERACTIVE_TIMER,INTERACTIVE_STOP_TIMER);
}
static void go(void){
 if(interactive.stage!=INTERACTIVE_RELEASED||interactive.active){interactive_error(INTERACTIVE_ORDER,INTERACTIVE_STOP_ENGINE);return;}
 service_input(sys_timer_ms(),1);
 if(interactive.stop_requested||!peaq_interactive_keys_status()->quiet_ready)return;
 /* Discard only the genuine preflight edges; never invent release events. */
 PeaqKeyFrame ignored;if(!peaq_interactive_keys_take(&ignored)){interactive_error(INTERACTIVE_KEY_READ,INTERACTIVE_STOP_INPUT);return;}
 if(ignored.held[0]||ignored.held[1]||ignored.gba||ignored.host)return;
 interactive.start_ms=interactive.last_request_ms=sys_timer_ms();
 idle_since=interactive.start_ms;
 interactive.active=1;interactive.stage=INTERACTIVE_RUNNING;interactive.flags|=INTERACTIVE_F_RUNNING;
 services.present=interactive_present;
}
static int main_reply(void){
 snapshot();interactive_snapshot();usb_gate();++interactive.reply_serial;
 return reply(PEAQ_INTERACTIVE_REPLY,&interactive,sizeof(interactive));
}
int peaq_interactive_dispatch(unsigned type,unsigned length){
 if(length)return BSL_REP_INVALID_CMD;
 if(type==0)return BSL_REP_ACK;
 int record=type>=PEAQ_INTERACTIVE_RECORD0&&type<PEAQ_INTERACTIVE_RECORD0+PEAQ_INTERACTIVE_RECORDS;
 if((type<PEAQ_INTERACTIVE_INFO||type>0x7c17u)&&!record)return BSL_REP_INVALID_CMD;
 interactive.last_command=type;cache.last_command=type;
 /* Count a valid request as a heartbeat, but first honor an already elapsed
  * deadline. A delayed host packet must not resurrect an expired session. */
 service_input(sys_timer_ms(),1);finish_stop();
 if(interactive.active)idle_since=interactive.last_request_ms=sys_timer_ms();
 if(record){
  if(!interactive.stopped)return BSL_REP_INVALID_CMD;
  const PeaqKeyRecord *r=peaq_interactive_keys_record(type-PEAQ_INTERACTIVE_RECORD0);
  return r?checked_reply(0xa0,r,sizeof(*r)):BSL_REP_INVALID_CMD;
 }
 if(type==PEAQ_INTERACTIVE_KEYS_INFO)return checked_reply(0x9f,peaq_interactive_keys_input(),sizeof(PeaqKeyInput));
 if(type==PEAQ_INTERACTIVE_FAULT0||type==PEAQ_INTERACTIVE_FAULT1)
  return checked_reply(type==PEAQ_INTERACTIVE_FAULT1?0x98:0x97,(const uint8_t*)(const void*)&peaq_fault+(type-PEAQ_INTERACTIVE_FAULT0)*192u,192);
 if(type==PEAQ_INTERACTIVE_INTRO_INFO){snapshot();return checked_reply(0x95,&intro,sizeof(intro));}
 if(type==PEAQ_INTERACTIVE_DISPLAY_INFO)return checked_reply(0x93,peaq_cold_display_status(),sizeof(PeaqFrameReport));
 if(type==PEAQ_INTERACTIVE_SD_INFO){snapshot();return checked_reply(0x96,&sd,sizeof(sd));}
 if(type==PEAQ_INTERACTIVE_SPEED_INFO)return checked_reply(0x94,&speed,sizeof(speed));
 if(type==0x7c17u)return saved_reply();
 if(type==0x7c16u)return adaptive_reply();
 if(type==0x7c15u)return checked_reply(0xa3,(const void*)&peaq_dc,sizeof(peaq_dc));
 if(type==PEAQ_INTERACTIVE_CACHE_INFO)return cache_reply();
 if(interactive.stopped||type==PEAQ_INTERACTIVE_INFO)return main_reply();
 if(type==PEAQ_INTERACTIVE_STOP)request_stop(INTERACTIVE_STOP_HOST);
 else if(type==PEAQ_INTERACTIVE_INIT){
  if(interactive.stage!=INTERACTIVE_IDLE)interactive_error(INTERACTIVE_ORDER,INTERACTIVE_STOP_ENGINE);
  else if(initialize())interactive.stage=INTERACTIVE_HW_READY;
 }else if(type==PEAQ_INTERACTIVE_SPEED){
  if(interactive.stage!=INTERACTIVE_HW_READY)interactive_error(INTERACTIVE_ORDER,INTERACTIVE_STOP_ENGINE);
  else if(check_speed())interactive.stage=INTERACTIVE_SPEED_READY;
 }else if(type==PEAQ_INTERACTIVE_LOAD){
  if(interactive.stage!=INTERACTIVE_SPEED_READY)interactive_error(INTERACTIVE_ORDER,INTERACTIVE_STOP_ENGINE);
  else if(start()){
   interactive.stage=INTERACTIVE_LOADED;interactive.flags|=INTERACTIVE_F_LOADED;
   cache.flags|=CACHE_F_STARTED;cache.stage=CACHE_STARTED;
  }
 }else if(type==PEAQ_INTERACTIVE_KEYS_INIT){
  if(interactive.stage!=INTERACTIVE_LOADED)interactive_error(INTERACTIVE_ORDER,INTERACTIVE_STOP_INPUT);
  else if(peaq_interactive_keys_init(intro.entered_sctlr,intro.entered_cpsr,sys_timer_ms())){
   interactive.stage=INTERACTIVE_KEYS_READY;interactive.flags|=INTERACTIVE_F_KEYS;
  }else interactive_error(INTERACTIVE_KEY_INIT,INTERACTIVE_STOP_INPUT);
 }else if(type==PEAQ_INTERACTIVE_QUIET)quiet_window();
 else if(type==PEAQ_INTERACTIVE_SMP){
  if(interactive.stage!=INTERACTIVE_QUIET_READY||!peaq_interactive_keys_status()->quiet_ready)interactive_error(INTERACTIVE_ORDER,INTERACTIVE_STOP_INPUT);
  else {smp_enable();if(!cache.error){interactive.stage=INTERACTIVE_SMP_ON;interactive.flags|=INTERACTIVE_F_SMP;}}
 }else if(type==PEAQ_INTERACTIVE_RECEIPT1){
  if(interactive.stage!=INTERACTIVE_SMP_ON)interactive_error(INTERACTIVE_ORDER,INTERACTIVE_STOP_CPU);
  else {receipt(1);if(!cache.error){interactive.stage=INTERACTIVE_RECEIPT_ONE;interactive.flags|=INTERACTIVE_F_RECEIPT1;}}
 }else if(type==PEAQ_INTERACTIVE_RECEIPT2){
  if(interactive.stage!=INTERACTIVE_RECEIPT_ONE)interactive_error(INTERACTIVE_ORDER,INTERACTIVE_STOP_CPU);
  else {receipt(2);if(!cache.error){interactive.stage=INTERACTIVE_RECEIPT_TWO;interactive.flags|=INTERACTIVE_F_RECEIPT2;}}
 }else if(type==PEAQ_INTERACTIVE_RELEASE){
  if(interactive.stage!=INTERACTIVE_RECEIPT_TWO)interactive_error(INTERACTIVE_ORDER,INTERACTIVE_STOP_CPU);
  else {release_cache();if(!cache.error){interactive.stage=INTERACTIVE_RELEASED;interactive.flags|=INTERACTIVE_F_RELEASED;}}
 }else if(type==PEAQ_INTERACTIVE_GO)go();
 else if(type==PEAQ_INTERACTIVE_STEP)step();
 if(cache.error&&!interactive.error)interactive_error(INTERACTIVE_CPU,INTERACTIVE_STOP_CPU);
 if(intro.error&&!interactive.error)interactive_error(INTERACTIVE_ENGINE,INTERACTIVE_STOP_ENGINE);
 finish_stop();
 if(type==PEAQ_INTERACTIVE_SPEED)return checked_reply(0x94,&speed,sizeof(speed));
 return main_reply();
}
void peaq_cold_main(uint32_t sctlr,uint32_t cpsr){
 memset(__bss_start,0,(size_t)(__bss_end-__bss_start));memset(&storage.fs,0,sizeof(storage.fs));stream_crc=~0u;saved_init();
 for(unsigned i=0;i<16;++i)storage.before[i]=storage.after[i]=runtime_arena.before[i]=runtime_arena.after[i]=GUARD;
 for(unsigned j=0;j<5;++j)for(unsigned i=0;i<16;++i)peaq_fault_stacks[j][i]=peaq_fault_stacks[j][144+i]=PEAQ_FAULT_GUARD;
 intro.magic=PEAQ_INTRO_MAGIC;intro.version=1;intro.bytes=sizeof(intro);intro.flags=(!(sctlr&0x1005u)?INTRO_F_COLD:0)|INTRO_F_BOARD;
 intro.chip_id=MEM4(0x402e00fcu);intro.entered_sctlr=sctlr;intro.entered_cpsr=cpsr;
 intro.frames_target=PEAQ_INTERACTIVE_FRAME_LIMIT;intro.batch_target=32;
 intro.arena_start=(uintptr_t)runtime_arena.bytes;intro.arena_bytes=sizeof(runtime_arena.bytes);
 intro.required_divider=4;intro.source_profile=PEAQ_SD_PROFILE_CRC32;
 sd.magic=PEAQ_SD_MAGIC;sd.version=1;sd.bytes=sizeof(sd);sd.flags=(!(sctlr&5u)?SD_F_COLD:0)|SD_F_BOARD;
 sd.chip_id=intro.chip_id;sd.entered_sctlr=sctlr;sd.entered_cpsr=cpsr;sd.profile_crc32=PEAQ_SD_PROFILE_CRC32;
 speed.magic=0x31475053u;speed.version=1;speed.bytes=sizeof(speed);speed.read_budget=PEAQ_ENGINE_READ_BUDGET;
 speed.required_crc=0x6337fde8u;speed.profile_crc32=PEAQ_SD_PROFILE_CRC32;
 cache.magic=PEAQ_CACHE_MAGIC;cache.version=1;cache.bytes=sizeof(cache);cache.flags=CACHE_F_COLD;
 cache.entered_sctlr=sctlr;cache.entered_actlr=peaq_cache_read_actlr();cache.midr=peaq_cache_read_midr();
 cache.frames_target=PEAQ_INTERACTIVE_FRAME_LIMIT;cache.batch_limit=32;all_guards();prepare_fault();
 interactive.magic=PEAQ_INTERACTIVE_MAGIC;interactive.version=1;interactive.bytes=sizeof(interactive);
 interactive.flags=INTERACTIVE_F_COLD|INTERACTIVE_F_NO_AUDIO;
 interactive.frame_limit=PEAQ_INTERACTIVE_FRAME_LIMIT;interactive.session_limit_ms=PEAQ_INTERACTIVE_SESSION_MS;
 interactive.request_limit=PEAQ_INTERACTIVE_REQUEST_LIMIT;interactive.host_idle_limit_ms=PEAQ_INTERACTIVE_HOST_IDLE_MS;
 interactive.quiet_required_ms=PEAQ_INTERACTIVE_QUIET_MS;interactive.owner_release_instruction_required=1;
 interactive.save_policy=2; /* audited fixed slots, two commits */
 dl_packet_init();usb_gate();dl_send_ack(BSL_REP_ACK);
 for(;;){uint8_t *p=dl_get_packet();int r=peaq_interactive_dispatch(READ16_BE(p),READ16_BE(p+2));if(r>=0){usb_gate();dl_send_ack(r);}}
}
