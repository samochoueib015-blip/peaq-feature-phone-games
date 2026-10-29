/* Separate guarded I-cache experiment. Reference code/data loading stays
 * unchanged. No device clock changes, firmware writes, card writes or OS exit.
 * Successful SMP activation is intentionally retained until battery reset. */
#include "intro_alignment.h"
#define peaq_intro_dispatch peaq_reference_intro_dispatch
#define peaq_cold_main peaq_reference_intro_main
#include "intro_diag.c"
#undef peaq_cold_main
#undef peaq_intro_dispatch
#include "first_step_fault.h"
#include "icache_intro.h"
#include <stddef.h>

static PeaqCacheReport cache;
volatile PeaqFirstStepFault peaq_fault;
uint32_t peaq_fault_stacks[5][160] __attribute__((aligned(64)));
uint32_t peaq_fault_bank_spsr[5];
static unsigned limit,maintenance_started,fault_attempts;
_Static_assert(sizeof(PeaqCacheReport)==192,"ICR1 layout");
_Static_assert(sizeof(PeaqFirstStepFault)==384,"frozen FST1 layout");
_Static_assert(offsetof(PeaqFirstStepFault,armed_sctlr)==44,"FST1 armed state");

static int cache_fail(unsigned e){
 if(!cache.error)cache.error=e;
 cache.stage=CACHE_FAILED;cache.terminal=1;cache.flags|=CACHE_F_FAILED;
 fail(18);return 0;
}
static void hard_fail(unsigned e){cache_fail(e);cache.usb_gate_ok=0;peaq_cache_halt();}
static int fault_guards(void){
 for(unsigned j=0;j<5;++j)for(unsigned i=0;i<16;++i)
  if(peaq_fault_stacks[j][i]!=PEAQ_FAULT_GUARD||peaq_fault_stacks[j][144+i]!=PEAQ_FAULT_GUARD)return 0;
 return 1;
}
static int all_guards(void){cache.guards_ok=guards()&&fault_guards();return cache.guards_ok?1:cache_fail(CACHE_GUARD);}
static uint32_t expected_actlr(void){return cache.entered_actlr|((cache.flags&CACHE_F_SMP)?0x40u:0u);}
static void read_cpu(void){
 cache.current_sctlr=peaq_cache_read_sctlr();cache.current_actlr=peaq_cache_read_actlr();
 cache.cpsr_after=peaq_cache_read_cpsr();
}
/* Called before EVERY USB packet/ACK, including invalid-command answers. */
static void usb_gate(void){
 read_cpu();cache.usb_gate_ok=0;
 if(cache.current_sctlr!=cache.entered_sctlr||cache.current_sctlr&0x1005u||
    cache.current_actlr!=expected_actlr()||(cache.cpsr_after&255u)!=0xdfu)
  hard_fail(CACHE_USB_STATE);
 if(!translation_restored())hard_fail(CACHE_USB_STATE);
 cache.usb_gate_ok=1;cache.flags|=CACHE_F_USB_SAFE;
}
/* Injected into a NEW generated copy of the frozen exception wrapper, at
 * peaq_fault_restore BEFORE its VBAR/original-SCTLR/banked-context restore.
 * This runs on the saved System stack after both normal and exception return.
 * Failure cannot return to that unwinder, an engine call, or a USB answer. */
static void prepare_fault(void){
 memset((void*)&peaq_fault,0,sizeof(peaq_fault));
 peaq_fault.magic=PEAQ_FAULT_MAGIC;peaq_fault.version=1;peaq_fault.bytes=sizeof(peaq_fault);
 peaq_fault.attempts=++fault_attempts;maintenance_started=0;
}
static int smp_target(void *arg){
 if(arg!=&runtime)return cache_fail(CACHE_CONTEXT);
 cache.cpsr_before=peaq_fault.cpsr_before;cache.armed_sctlr=peaq_fault.armed_sctlr;
 if(peaq_cache_read_sctlr()!=peaq_fault.armed_sctlr||peaq_cache_read_actlr()!=cache.entered_actlr)return cache_fail(CACHE_CONTEXT);
 peaq_fault.breadcrumb=10;
 peaq_cache_write_actlr(cache.entered_actlr|0x40u);
 cache.smp_readback=peaq_cache_read_actlr();
 if(cache.smp_readback==(cache.entered_actlr|0x40u)){
  cache.flags|=CACHE_F_SMP|CACHE_F_POWER_CYCLE;cache.awaiting_power_cycle=1;
 }else return cache_fail(CACHE_READBACK);
 if(peaq_cache_read_sctlr()!=peaq_fault.armed_sctlr)return cache_fail(CACHE_READBACK);
 return 1;
}
static int finish_fault(int completed){
 peaq_fault.stack_guards_ok=fault_guards();if(peaq_fault.stack_guards_ok)peaq_fault.flags|=FAULT_GUARDS;
 cache.control_restored=peaq_fault.control_restored;
 cache.outer_restored_sctlr=peaq_fault.restored_sctlr;
 if(peaq_fault.flags&FAULT_CAPTURED){
  ++cache.cpu_faults;cache.flags|=CACHE_F_CPU_FAULT;
  cache.exception_kind=peaq_fault.exception_kind;cache.fault_pc=peaq_fault.fault_pc;
  cache.dfsr=peaq_fault.dfsr;cache.dfar=peaq_fault.dfar;
  cache_fail(CACHE_CPU_FAULT);
 }
 if(!peaq_fault.control_restored)hard_fail(CACHE_STATE);
 if(completed<=0||peaq_fault.error)cache_fail(CACHE_CPU_FAULT);
 if(!all_guards())cache_fail(CACHE_GUARD);
 if(cache.error)return 0;
 if(peaq_fault.step_result!=1)return cache_fail(CACHE_STATE);
 return 1;
}
static void smp_enable(void){
 if(cache.terminal||intro.error)return;
 if(!(intro.flags&INTRO_F_STARTED)||intro.frames)return (void)cache_fail(CACHE_ORDER);
 if(cache.smp_attempts)return (void)cache_fail(CACHE_REPEAT);
 if(cache.midr!=0x410fc075u||cache.entered_actlr!=0x6000u)return (void)cache_fail(CACHE_CONTEXT);
 if(!all_guards())return;
 ++cache.smp_attempts;prepare_fault();
 uint32_t t=sys_timer_ms();
 int result=peaq_fault_call(smp_target,&runtime);
 peaq_fault.step_ms=sys_timer_ms()-t;
 if(finish_fault(result))cache.stage=CACHE_SMP_ON;
}
static void receipt(unsigned n){
 if(cache.terminal||intro.error)return;
 if(!(cache.flags&CACHE_F_SMP)||cache.release_count||intro.frames||
    (n==2&&cache.gate_receipts<1))return (void)cache_fail(CACHE_ORDER);
 usb_gate();
 if(n>cache.gate_receipts)cache.gate_receipts=n;
 cache.flags|=n==1?CACHE_F_RECEIPT1:CACHE_F_RECEIPT2;
 cache.stage=cache.gate_receipts==2?CACHE_RECEIPT_TWO:CACHE_RECEIPT_ONE;
}
static void release_cache(void){
 if(cache.terminal||intro.error)return;
 if(cache.gate_receipts!=2||!(cache.flags&CACHE_F_SMP)||intro.frames)return (void)cache_fail(CACHE_ORDER);
 if(cache.release_count)return (void)cache_fail(CACHE_REPEAT);
 usb_gate();if(!cached_prepare())return;++cache.release_count;cache.flags|=CACHE_F_RELEASED;cache.stage=CACHE_RELEASED;
}
static void cache_batch(unsigned first){
 if(cache.terminal||intro.error)return;
 if(!cache.release_count||cache.gate_receipts!=2)return (void)cache_fail(CACHE_ORDER);
 if((first&&intro.frames)||(!first&&!intro.frames)||intro.frames>=240u)return (void)cache_fail(CACHE_ORDER);
 if(!all_guards()||!outer_result())return;
 usb_gate();limit=first?1:16;prepare_fault();
 uint32_t before=intro.frames,t=sys_timer_ms();
 ++cache.batch_count;++intro.batch_count;cache.stage=CACHE_RUNNING;intro.stage=INTRO_RUNNING;
 int completed=peaq_fault_call(cached_frames_target,&runtime);
 cache.last_batch_ms=sys_timer_ms()-t;cache.total_batch_ms+=cache.last_batch_ms;
 peaq_fault.step_ms=cache.last_batch_ms;
 cache.last_batch_frames=intro.frames-before;intro.last_batch_ms=cache.last_batch_ms;intro.wall_ms=sys_timer_ms()-started_ms;
 int ok=finish_fault(completed);
 if(!ok){peaq_emerald_runtime_pause(&runtime,1,logical_ms);return;}
 if(intro.frames==240u){
  peaq_emerald_runtime_pause(&runtime,1,logical_ms);intro.flags|=INTRO_F_COMPLETE;intro.stage=INTRO_COMPLETE;
  cache.flags|=CACHE_F_COMPLETE;cache.stage=CACHE_COMPLETE;
 }else cache.stage=CACHE_RELEASED;
}
static int checked_reply(unsigned kind,const void *p,unsigned bytes){usb_gate();return reply(kind,p,bytes);}
static int cache_reply(void){
 snapshot();cache.frames=intro.frames;all_guards();usb_gate();++cache.reply_serial;
 return reply(PEAQ_CACHE_REPLY,&cache,sizeof(cache));
}
int peaq_unused_cache_dispatch(unsigned type,unsigned length){
 if(length)return BSL_REP_INVALID_CMD;
 if(type==0)return BSL_REP_ACK;
 if(type==0x7910u)return checked_reply(0xa3,(const void*)&peaq_dc,sizeof(peaq_dc));
 if(type<PEAQ_CACHE_INFO||type>PEAQ_CACHE_FAULT1)return BSL_REP_INVALID_CMD;
 cache.last_command=type;
 if(type==PEAQ_CACHE_INIT)initialize();
 else if(type==PEAQ_CACHE_SPEED)check_speed();
 else if(type==PEAQ_CACHE_START){if(start()){cache.flags|=CACHE_F_STARTED;cache.stage=CACHE_STARTED;}}
 else if(type==PEAQ_CACHE_SMP)smp_enable();
 else if(type==PEAQ_CACHE_RECEIPT1)receipt(1);
 else if(type==PEAQ_CACHE_RECEIPT2)receipt(2);
 else if(type==PEAQ_CACHE_RELEASE)release_cache();
 else if(type==PEAQ_CACHE_FIRST)cache_batch(1);
 else if(type==PEAQ_CACHE_BATCH)cache_batch(0);
 if(intro.error&&!cache.error)cache_fail(CACHE_ENGINE);
 if(type==PEAQ_CACHE_INFO||(type>=PEAQ_CACHE_SMP&&type<=PEAQ_CACHE_BATCH))return cache_reply();
 snapshot();
 if(type==PEAQ_CACHE_FAULT0||type==PEAQ_CACHE_FAULT1){unsigned part=type-PEAQ_CACHE_FAULT0;return checked_reply(part?0x98:0x97,(const uint8_t*)(const void*)&peaq_fault+192u*part,192);}
 if(type==PEAQ_CACHE_DISPLAY_INFO)return checked_reply(0x93,peaq_cold_display_status(),sizeof(PeaqFrameReport));
 if(type==PEAQ_CACHE_SPEED||type==PEAQ_CACHE_SPEED_INFO)return checked_reply(0x94,&speed,sizeof(speed));
 if(type==PEAQ_CACHE_SD_INFO)return checked_reply(0x96,&sd,sizeof(sd));
 return checked_reply(0x95,&intro,sizeof(intro));
}
void peaq_unused_cache_main(uint32_t sctlr,uint32_t cpsr){
 memset(__bss_start,0,(size_t)(__bss_end-__bss_start));memset(&storage.fs,0,sizeof(storage.fs));stream_crc=~0u;
 for(unsigned i=0;i<16;++i)storage.before[i]=storage.after[i]=runtime_arena.before[i]=runtime_arena.after[i]=GUARD;
 for(unsigned j=0;j<5;++j)for(unsigned i=0;i<16;++i)peaq_fault_stacks[j][i]=peaq_fault_stacks[j][144+i]=PEAQ_FAULT_GUARD;
 intro.magic=PEAQ_INTRO_MAGIC;intro.version=1;intro.bytes=sizeof(intro);intro.flags=(!(sctlr&0x1005u)?INTRO_F_COLD:0)|INTRO_F_BOARD;
 intro.chip_id=MEM4(0x402e00fcu);intro.entered_sctlr=sctlr;intro.entered_cpsr=cpsr;
 intro.frames_target=240;intro.batch_target=16;intro.arena_start=(uintptr_t)runtime_arena.bytes;intro.arena_bytes=sizeof(runtime_arena.bytes);
 intro.required_divider=4;intro.source_profile=PEAQ_SD_PROFILE_CRC32;
 sd.magic=PEAQ_SD_MAGIC;sd.version=1;sd.bytes=sizeof(sd);sd.flags=(!(sctlr&5u)?SD_F_COLD:0)|SD_F_BOARD;
 sd.chip_id=intro.chip_id;sd.entered_sctlr=sctlr;sd.entered_cpsr=cpsr;sd.profile_crc32=PEAQ_SD_PROFILE_CRC32;
 speed.magic=0x31475053u;speed.version=1;speed.bytes=sizeof(speed);speed.read_budget=PEAQ_ENGINE_READ_BUDGET;
 speed.required_crc=0x6337fde8u;speed.profile_crc32=PEAQ_SD_PROFILE_CRC32;
 cache.magic=PEAQ_CACHE_MAGIC;cache.version=1;cache.bytes=sizeof(cache);cache.flags=CACHE_F_COLD;
 cache.entered_sctlr=sctlr;cache.entered_actlr=peaq_cache_read_actlr();cache.midr=peaq_cache_read_midr();
 cache.frames_target=240;cache.batch_limit=16;all_guards();prepare_fault();
 dl_packet_init();usb_gate();dl_send_ack(BSL_REP_ACK);
 for(;;){uint8_t *p=dl_get_packet();int r=peaq_cache_dispatch(READ16_BE(p),READ16_BE(p+2));if(r>=0){usb_gate();dl_send_ack(r);}}
}
