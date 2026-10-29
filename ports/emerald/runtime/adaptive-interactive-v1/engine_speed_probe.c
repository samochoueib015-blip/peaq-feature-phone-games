/* Fixed comparison file, twice before and twice after one source-proven
 * divider change. Frozen FAT fixture checker validates every deterministic
 * byte, chain, length and CRC. No arbitrary file or card write interface.
 */
#include "engine_speed_probe.h"
#include "engine_load_manifest.h"
#include "peaq_sd_board_profile.h"
#define REG(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define EXPECTED 0x6337fde8u
extern uint32_t sys_timer_ms(void);
_Static_assert(sizeof(PeaqEngineSpeedReport)==128,"SPG1 report layout");
static int fail(PeaqEngineSpeedReport *p,unsigned n){if(!p->error)p->error=n;p->stage=4;return 0;}
int peaq_engine_speed_probe(PeaqSdReport *s,PeaqEngineSpeedReport *p){
 uint32_t start=sys_timer_ms(),t;int ok=0;
 if(p->error)return 0;
 if(p->count)return fail(p,2);
 p->magic=0x31475053u;p->version=1;p->bytes=sizeof(*p);p->read_budget=PEAQ_ENGINE_READ_BUDGET;
 p->required_crc=EXPECTED;p->profile_crc32=PEAQ_SD_PROFILE_CRC32;
 if(!(s->flags&SD_F_CARD)||s->error||s->poisoned)return fail(p,1);
 ++p->count;p->stage=1;p->clock_before=REG(0x2030002cu);p->source_selector=REG(0x2150006cu)&7u;p->bus_control=REG(0x20300028u);
 p->reads_before=s->reads;p->commands_before=s->commands;
 if(!peaq_sd_sector0(s)){fail(p,3);goto done;}
 t=sys_timer_ms();
 if(!peaq_sd_file(s)){fail(p,3);goto done;}
 p->baseline_ms=sys_timer_ms()-t;p->baseline_bytes=s->file_bytes;p->baseline_crc0=s->file_crc0;p->baseline_crc1=s->file_crc1;
 if(p->baseline_bytes!=32768u||p->baseline_crc0!=EXPECTED||p->baseline_crc1!=EXPECTED){fail(p,3);goto done;}
 p->flags|=1;p->stage=2;
 if(!peaq_sd_fixed_speed(s)){fail(p,4);goto done;}
 p->clock_after=s->clock_control;
 /* Reuse the fixed read-only checker once more in the same exclusively owned
  * session. Counters, read budget, card identity and permanent DMA stay live.
  */
 s->file_count=0;s->file_crc0=s->file_crc1=0;s->flags&=~SD_F_FILE;t=sys_timer_ms();
 if(!peaq_sd_file(s)){fail(p,5);goto done;}
 p->fast_ms=sys_timer_ms()-t;p->fast_bytes=s->file_bytes;p->fast_crc0=s->file_crc0;p->fast_crc1=s->file_crc1;
 if(p->fast_bytes!=32768u||p->fast_crc0!=EXPECTED||p->fast_crc1!=EXPECTED){fail(p,5);goto done;}
 p->flags|=2;p->stage=3;p->guards_ok=1;ok=1;
done:
 p->total_ms=sys_timer_ms()-start;p->reads_after=s->reads;p->commands_after=s->commands;
 p->sd_error=s->error;p->poisoned=s->poisoned;p->cleanup=s->cleanup;return ok;
}
