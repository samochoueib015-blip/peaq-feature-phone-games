/* Separate v3: exact physical RESET_CTRL30300 plus original preserved-bit init.
 * V1 sources and all qualified artifacts remain unchanged. */
/* Separate cold RAM keypad diagnostic. No engine, LCD, SD, NAND, EIC or ADI.
 * Original source order: RAW, CLR read/OR/ACK, press suppression reads, then
 * two STATUS reads for each deferred event. The scheduler is replaced by
 * bounded polling; this image measures the resulting physical behavior.
 */
#include "keypad_capture_v3.h"
#include "cold_key_input.h"
#include "peaq_keypad_board_profile.h"
#include "common.h"
#include "packet.h"
#define REG(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define KPD 0x40250000u
#define CTRL 0x001f3f05u
#define RESET_CTRL 0x00030300u
#define ORIGINAL_MASK 0x001c3c04u
#define GUARD 0x6b504431u
extern uint8_t __bss_start[],__bss_end[],__image_end[];
static PeaqKeyDiag report;
static struct {uint32_t pre[16];PeaqKeyInput state;uint32_t post[16];} input;
static struct {uint32_t pre[16];PeaqKeyRecord rows[KEY_DIAG_RECORDS];uint32_t post[16];} logbook;
static uint32_t enabled,phase;
_Static_assert(sizeof(PeaqKeyDiag)==208,"KPD3 wire size");
_Static_assert(sizeof(PeaqKeyRecord)==144,"KPR3 wire size");
uint32_t sys_timer_ms(void){return REG(0x4023000cu);}
static uint32_t cpu_control(void){uint32_t v;__asm__ volatile("mrc p15,0,%0,c1,c0,0":"=r"(v));return v;}
static uint32_t cpu_status(void){uint32_t v;__asm__ volatile("mrs %0,cpsr":"=r"(v));return v;}
static int guards(void){
 for(unsigned i=0;i<16;++i)if(input.pre[i]!=GUARD||input.post[i]!=GUARD||logbook.pre[i]!=GUARD||logbook.post[i]!=GUARD)return 0;
 return 1;
}
static unsigned popcount(uint32_t v){unsigned n=0;while(v){n+=v&1u;v>>=1;}return n;}
static void sync_input(void){
 PeaqKeyInput *s=&input.state;
 report.held[0]=s->held[0];report.held[1]=s->held[1];report.gba=s->gba;report.host=s->host;
 report.presses=s->press_count;report.releases=s->release_count;report.longs=s->long_count;
 report.invalid=s->invalid_count;
 unsigned n=popcount(s->held[0])+popcount(s->held[1]);if(n>report.max_held)report.max_held=n;
}
static void stop(void){
 if(enabled){
  uint32_t v=REG(KPD);report.control=v;
  /* Original stop clears bit0. Refuse an unexpected register shape. */
  if(v==CTRL){REG(KPD)=v&~1u;__asm__ volatile("dsb sy":::"memory");report.control=REG(KPD);if(report.control!=(CTRL&~1u)&&!report.error){report.error=KPD_CONTROL;report.error_phase=KPD_PHASE_STOP;}}
  else if(!report.error){report.error=KPD_CONTROL;report.error_phase=KPD_PHASE_STOP;}
  enabled=0;
 }
 report.stopped=1;report.flags|=KPD_F_STOP;
 report.stage=report.error?KPD_STAGE_FAILED:KPD_STAGE_STOP;
 if(report.error)report.flags|=KPD_F_ERROR;
}
static int fail(unsigned code){
 if(!report.error){report.error=code;report.error_phase=phase;}
 report.flags|=KPD_F_ERROR;stop();return 0;
}
static void reset_delay(void){
 /* Exact original bounded1000-iteration reset wait; no millisecond claim. */
 __asm__ volatile("mov r0,#0\nmovw r1,#1000\n1: adds r0,r0,#1\ncmp r0,r1\nblo 1b":::"r0","r1","cc","memory");
}
static int initialize(void){
 phase=KPD_PHASE_PRECONDITIONS;
 if((cpu_control()&0x1005u)||(cpu_status()&0xdfu)!=0xdfu)return fail(KPD_COLD_REQUIRED);
 if(report.chip_id!=0x98180001u)return fail(KPD_CHIP_MISMATCH);
 if(!guards())return fail(KPD_GUARD);
 uint32_t rows=0,columns=0,count=0;
 for(unsigned i=0;i<64;++i)if(peaq_key_logical_code(i)!=0xffffu){rows|=1u<<(i&7u);columns|=1u<<(i>>3);++count;}
 /* Original helper calls these columns3f / rows1f in its own orientation. */
 if(rows!=0x1fu||columns!=0x3fu||count!=26)return fail(KPD_MATRIX);
 ++report.init_count;report.stage=KPD_STAGE_PADS;phase=KPD_PHASE_PADS;
 REG(0x402e1000u)=0x100000u;REG(0x402e10b0u)=0x1000u;
 peaq_shared_pins_init();
 REG(0x402a0018u)&=0xe0000000u;
 report.stage=KPD_STAGE_RESET;phase=KPD_PHASE_RESET;
 REG(0x402e1000u)=0x100u;REG(0x402e1010u)=2;
 REG(0x402e1008u)=0x100u;reset_delay();REG(0x402e2008u)=0x100u;reset_delay();
 /* Physically observed reset value, measured before configuration in v2. */
 phase=KPD_PHASE_RESET_CHECK;report.reset_control=REG(KPD);report.control=report.reset_control;
 if(report.reset_control!=RESET_CTRL)return fail(KPD_CONTROL);
 /* Preserve original ordering: CLR, divider, read/clear sleep bit1,
  * reread/OR original full-matrix mask, other fields, reread/OR enable.
  * Every original read is constrained to the single source+device value. */
 REG(KPD+0x10)=0xfffu;REG(KPD+0x28)=1;
 phase=KPD_PHASE_FIRST_CTRL;uint32_t v=REG(KPD);report.control=v;
 if(v!=RESET_CTRL)return fail(KPD_CONTROL);
 report.first_control_write=v&~2u;REG(KPD)=report.first_control_write;
 phase=KPD_PHASE_MASK_OR;v=REG(KPD);report.control=v;
 if(v!=RESET_CTRL)return fail(KPD_CONTROL);
 REG(KPD)=v|ORIGINAL_MASK;
 phase=KPD_PHASE_AUX_CONFIG;REG(KPD+0x1c)=16;REG(KPD+4)=0xfffu;REG(KPD+0x18)=0xffffu;
 phase=KPD_PHASE_ENABLE_CHECK;v=REG(KPD);report.control=v;
 if(v!=(CTRL&~1u))return fail(KPD_CONTROL);
 REG(KPD)=v|1u;enabled=1;__asm__ volatile("dsb sy":::"memory");
 phase=KPD_PHASE_CONFIG_READBACK;
 report.configured_control=REG(KPD);report.control=report.configured_control;
 report.divider=REG(KPD+0x28);report.debounce=REG(KPD+0x1c);
 report.polarity=REG(KPD+0x18);report.interrupt_enable=REG(KPD+4);
 if(report.control!=CTRL||report.divider!=1||report.debounce!=16||report.polarity!=0xffffu||report.interrupt_enable!=0xfffu)return fail(KPD_CONTROL);
 report.flags|=KPD_F_INIT;report.stage=KPD_STAGE_READY;return 1;
}
static int capture(uint32_t raw,uint32_t elapsed){
 if(report.record_count==KEY_DIAG_RECORDS)return fail(KPD_LOG_FULL);
 PeaqKeyRecord *r=&logbook.rows[report.record_count++];
 r->magic=KEY_DIAG_RECORD_MAGIC;r->sequence=report.record_count;r->elapsed_ms=elapsed;r->raw=raw;r->poll_index=report.poll_count;
 report.last_raw=raw;report.flags|=KPD_F_EVENT;
 if(raw&~0xfffu){r->error=KPD_BAD_RAW;return fail(r->error);}
 report.coalesced+=popcount(raw&(raw>>4)&15u);
 r->clear_read=REG(KPD+0x10);report.last_clear=r->clear_read;
 if(r->clear_read&~0xfffu){r->error=KPD_BAD_CLEAR;return fail(r->error);}
 r->ack_write=r->clear_read|0xfffu;REG(KPD+0x10)=r->ack_write;++report.ack_count;
 __asm__ volatile("dsb sy":::"memory");
 /* Capture all original ISR press-suppression reads before any converter. */
 for(unsigned i=0;i<4;++i)if(raw&(1u<<i)){
  r->suppress_status[i]=REG(KPD+0x2c);report.last_status=r->suppress_status[i];
  if(r->suppress_status[i]&8u){r->suppressed_mask|=1u<<i;++report.suppressed;}
 }
 r->accepted_raw=(raw&0xffu)&~r->suppressed_mask;
 for(unsigned i=0;i<8;++i)if(r->accepted_raw&(1u<<i)){
  unsigned slot=i&3u,shift=8u*slot;
  r->status_first[i]=REG(KPD+0x2c);r->status_second[i]=REG(KPD+0x2c);report.last_status=r->status_second[i];
  uint32_t a=r->status_first[i]>>shift,b=r->status_second[i]>>shift;
  /* The original extracts high3 bits from first and low3 from second.
   * Reject tearing instead of synthesizing a different matrix intersection. */
  if((a^b)&0x77u){r->error=KPD_TORN_SLOT;break;}
  uint32_t byte=(a&0x70u)|(b&7u);
  if(peaq_key_logical_code(peaq_key_matrix_index(byte))==0xffffu){r->error=KPD_UNMAPPED;break;}
  /* Press suppression has already been applied in the original ISR order.
   * Clear globalbit3 in this constructed single-event decoder snapshot. */
  if(peaq_key_input_feed(&input.state,1u<<i,byte<<shift)!=1){r->error=KPD_UNMAPPED;break;}
  sync_input();
 }
 if(!r->error)peaq_key_input_feed(&input.state,raw&0xf00u,0);
 sync_input();r->held[0]=report.held[0];r->held[1]=report.held[1];r->gba=report.gba;r->host=report.host;
 r->flags=input.state.flags;
 if(r->error)return fail(r->error);
 if(!guards()){r->error=KPD_GUARD;return fail(r->error);}
 return 1;
}
static int window(void){
 phase=KPD_PHASE_CAPTURE;
 if(!guards())return fail(KPD_GUARD);
 if(cpu_control()&0x1005u)return fail(KPD_COLD_REQUIRED);
 report.stage=KPD_STAGE_CAPTURE;++report.window_count;
 uint32_t start=sys_timer_ms(),last=start,stalled=0,polls;
 for(polls=0;polls<KEY_DIAG_MAX_POLLS;++polls){
  uint32_t t=sys_timer_ms();++report.poll_count;
  if((t-last)&0x80000000u){report.capture_ms+=t-start;return fail(KPD_TIMER);}
  if(t-start>=KEY_DIAG_WINDOW_MS){report.capture_ms+=t-start;break;}
  if(t!=last){last=t;stalled=0;}else if(++stalled>=KEY_DIAG_STALLED_POLLS)return fail(KPD_TIMER);
  uint32_t raw=REG(KPD+8);
  if(raw&&!capture(raw,report.capture_ms+t-start))return 0;
 }
 if(polls==KEY_DIAG_MAX_POLLS)return fail(KPD_TIMER);
 if(!guards())return fail(KPD_GUARD);
 if(report.window_count==KEY_DIAG_WINDOWS)stop();else report.stage=KPD_STAGE_READY;
 return 1;
}
static void send_bytes(unsigned kind,const void *data,unsigned size){
 uint8_t *p=dl_send_buf();WRITE16_BE(p,kind);WRITE16_BE(p+2,size);memcpy(p+4,data,size);dl_send_packet(p);
}
static void send_report(void){report.guard_ok=guards();report.timer_ms=sys_timer_ms();report.current_cpsr=cpu_status();send_bytes(0x9e,&report,sizeof(report));}
int peaq_key_capture_v3_dispatch(unsigned type,unsigned length){
 if(length)return BSL_REP_INVALID_CMD;
 if(type==0)return BSL_REP_ACK;
 if(type==KEY_DIAG_INFO){send_report();return -1;}
 if(type==KEY_DIAG_INPUT){send_bytes(0x9f,&input.state,sizeof(input.state));return -1;}
 if(type>=KEY_DIAG_RECORD0&&type<KEY_DIAG_RECORD0+KEY_DIAG_RECORDS){
  unsigned i=type-KEY_DIAG_RECORD0;if(!report.stopped||i>=report.record_count)return BSL_REP_INVALID_CMD;
  send_bytes(0xa0,&logbook.rows[i],sizeof(PeaqKeyRecord));return -1;
 }
 if(type==KEY_DIAG_INIT){if(report.init_count||report.stopped||report.error)return BSL_REP_INVALID_CMD;initialize();}
 else if(type==KEY_DIAG_WINDOW){if(!enabled||report.stopped||report.error||report.window_count>=KEY_DIAG_WINDOWS)return BSL_REP_INVALID_CMD;window();}
 else if(type==KEY_DIAG_STOP){if(!enabled||report.stopped)return BSL_REP_INVALID_CMD;stop();}
 else return BSL_REP_INVALID_CMD;
 send_report();return -1;
}
void peaq_cold_main(uint32_t sctlr,uint32_t cpsr){
 memset(__bss_start,0,(size_t)(__bss_end-__bss_start));
 for(unsigned i=0;i<16;++i)input.pre[i]=input.post[i]=logbook.pre[i]=logbook.post[i]=GUARD;
 peaq_key_input_init(&input.state);
 report.magic=KEY_DIAG_MAGIC;report.version=3;report.bytes=sizeof(report);
 report.sctlr=sctlr;report.cpsr=cpsr;report.chip_id=REG(0x402e00fcu);
 report.flags=KPD_F_BOARD|(!(sctlr&0x1005u)?KPD_F_COLD:0);
 report.pin_count=PEAQ_KPD_PIN_COUNT;report.pin_crc=PEAQ_KPD_PIN_CRC;report.key_crc=PEAQ_KPD_KEY_CRC;report.key_entries=64;
 report.window_limit=KEY_DIAG_WINDOWS;report.window_ms=KEY_DIAG_WINDOW_MS;report.record_limit=KEY_DIAG_RECORDS;
 report.image_end=(uintptr_t)__image_end;report.bss_end=(uintptr_t)__bss_end;
 dl_packet_init();dl_send_ack(BSL_REP_ACK);
 for(;;){uint8_t *p=dl_get_packet();int reply=peaq_key_capture_v3_dispatch(READ16_BE(p),READ16_BE(p+2));if(reply>=0)dl_send_ack(reply);}
}
