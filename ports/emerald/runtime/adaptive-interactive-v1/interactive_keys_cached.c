extern int peaq_cached_input_context(void);
/* Private reuse of the unchanged, physically tested v3 decoder/acquisition.
 * One zeroed scratch record feeds a separate lifetime64-slot ring. The old
 * 64-record and256-window stop conditions are never used by this service. */
#include "keypad_capture_v3.h"
#undef KEY_DIAG_RECORDS
#define KEY_DIAG_RECORDS 1u
#define report peaq_key_v3_report
#define sys_timer_ms peaq_interactive_key_timer_ms
#define peaq_cold_main peaq_unused_key_v3_main
#define peaq_key_capture_v3_dispatch peaq_unused_key_v3_dispatch
#include "keypad_capture_v3.c"
#undef peaq_key_capture_v3_dispatch
#undef peaq_cold_main
#undef sys_timer_ms
#include "interactive_keys.h"
#include "interactive_diag.h"
static PeaqInteractiveKeys k;
static struct {uint32_t pre[16];PeaqKeyRecord rows[64];uint32_t post[16];} ring;
static uint32_t born,last_poll,quiet_since;

int peaq_interactive_keys_guards(void){
 if(!guards())return 0;
 for(unsigned i=0;i<16;++i)if(ring.pre[i]!=GUARD||ring.post[i]!=GUARD)return 0;
 return 1;
}
int peaq_interactive_keys_init(uint32_t sctlr,uint32_t cpsr,uint32_t now){
 if(k.initialized||k.stopped)return 0;
 memset(&report,0,sizeof(report));memset(&input,0,sizeof(input));memset(&logbook,0,sizeof(logbook));
 memset(&ring,0,sizeof(ring));
 for(unsigned i=0;i<16;++i)input.pre[i]=input.post[i]=logbook.pre[i]=logbook.post[i]=ring.pre[i]=ring.post[i]=GUARD;
 peaq_key_input_init(&input.state);
 report.magic=KEY_DIAG_MAGIC;report.version=3;report.bytes=sizeof(report);
 report.sctlr=sctlr;report.cpsr=cpsr;report.chip_id=REG(0x402e00fcu);
 report.flags=KPD_F_BOARD|(!(sctlr&0x1005u)?KPD_F_COLD:0);
 born=last_poll=quiet_since=now;k.initialized=1;
 int ok=initialize();k.error=report.error;
 if(!ok)k.stopped=1;
 return ok&&peaq_interactive_keys_guards();
}
int peaq_interactive_keys_poll(uint32_t now){
 if(!k.initialized||k.stopped||k.error)return 0;
 uint32_t delta=now-last_poll;
 if(delta&0x80000000u){k.error=KPD_TIMER;return 0;}
 if(delta>k.max_gap_ms)k.max_gap_ms=delta;
 last_poll=now;
 if(!peaq_interactive_keys_guards()){k.error=KPD_GUARD;return 0;}
 /* Polling may also occur inside the qualified I-only Present boundary.
  * M/C remain forbidden; no cache/control operation is done by this helper. */
 if(!peaq_cached_input_context()){k.error=KPD_COLD_REQUIRED;return 0;}
 for(unsigned i=0;i<4;++i){
  report.poll_count=++k.polls;
  uint32_t raw=REG(KPD+8);if(!raw)break;
  quiet_since=now;
  report.record_count=0;memset(logbook.rows,0,sizeof(logbook.rows));
  int ok=capture(raw,now-born);
  if(report.record_count){
   PeaqKeyRecord *r=&ring.rows[k.records&63u];memcpy(r,&logbook.rows[0],sizeof(*r));
   if(k.records>=64u)++k.overwrites;
   r->sequence=++k.records;
  }
  if(!ok){k.error=report.error?report.error:KPD_GUARD;return 0;}
 }
 if(input.state.held[0]||input.state.held[1]||input.state.gba||input.state.host)quiet_since=now;
 k.quiet_ms=now-quiet_since;k.quiet_ready=k.quiet_ms>=PEAQ_INTERACTIVE_QUIET_MS;
 return 1;
}
int peaq_interactive_keys_take(PeaqKeyFrame *f){return peaq_key_input_take(&input.state,f);}
void peaq_interactive_keys_stop(void){
 if(!k.initialized||k.stopped)return;
 stop();peaq_key_input_release_all(&input.state);sync_input();
 k.error=report.error;k.stopped=1;k.quiet_ready=0;k.quiet_ms=0;
}
const PeaqInteractiveKeys *peaq_interactive_keys_status(void){return &k;}
const PeaqKeyInput *peaq_interactive_keys_input(void){return &input.state;}
const PeaqKeyRecord *peaq_interactive_keys_record(unsigned i){
 if(!k.stopped||i>=64u||!ring.rows[i].sequence)return 0;
 return &ring.rows[i];
}
