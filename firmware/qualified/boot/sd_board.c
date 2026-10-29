/* Separate read-only cold SDIO0 diagnostic. Register layout/command format
 * derive from pinned Unlicense FPDoom ums9117/sdio.c. Board pads and LDOs
 * derive from this handset's original image. No generic pad initialization,
 * card write/erase commands, voltage writes, 4-bit/high-speed negotiation,
 * NAND, LCD or OS callbacks. This does not replace the qualified LCD image.
 */
#include "sd_diag.h"
#include "peaq_sd_board_profile.h"
#include <stddef.h>
#define REG(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define SD(off) REG(0x20300000u+(off))
#define POLLS 1000000u
#define ERRORS 0x1b7f8000u
#define R1_ERRORS 0xffffa008u
#define CHECKS ((1u<<20)|(1u<<19))
#define DMA_GUARD 0xb59a3147u
/* Permanently owned buffer, never a caller/stack/game address. A timeout
 * poisons the controller and prohibits further commands, even if reset fails.
 * It remains allocated for the lifetime of this RAM image.
 */
static struct { uint32_t before[16];uint8_t bytes[512];uint32_t after[16]; }
 dma __attribute__((aligned(64)));
static PeaqSdReport *state;
static unsigned controller_live;
static uint32_t now(void) { return REG(0x4023000cu); }
static int fail(unsigned error) {
 if(!state->error) state->error=error;
 return 0;
}
static int elapsed(uint32_t start,uint32_t *previous,uint32_t ms) {
 uint32_t t=now();
 ++state->total_polls;
 if((t-*previous)&0x80000000u) { fail(SD_TIMER_ERROR);return -1; }
 *previous=t;return t-start>=ms;
}
static int delay(uint32_t ms) {
 uint32_t start=now(),prev=start,last=start,stalled=0;
 unsigned i;
 for(i=0;i<64000000u;++i) {
  int done=elapsed(start,&prev,ms);
  if(done) return done>0;
  if(prev==last) { if(++stalled==POLLS) return fail(SD_TIMER_ERROR); }
  else { stalled=0;last=prev; }
 }
 return fail(SD_TIMER_ERROR);
}
static int wait_reg(uint32_t address,uint32_t mask,uint32_t expected,uint32_t ms,unsigned error) {
 uint32_t start=now(),prev=start;unsigned i;
 for(i=0;i<POLLS;++i) {
  if((REG(address)&mask)==expected) return 1;
  if(elapsed(start,&prev,ms)) break;
 }
 return fail(error);
}
static int adi_read(uint32_t address,uint32_t *v) {
 uint32_t start=now(),prev=start;unsigned i;
 REG(0x40600028u)=address;
 for(i=0;i<4096u;++i) {
  uint32_t n=REG(0x4060002cu);
  if(!(n&0x80000000u)) { *v=n&65535u;return 1; }
  if(elapsed(start,&prev,10)) break;
 }
 return fail(SD_ADI_TIMEOUT);
}
static int adi_write(uint32_t address,uint32_t value) {
 if(!wait_reg(0x40600030u,0x800u,0,10,SD_ADI_TIMEOUT)) return 0;
 REG(address)=value&65535u;return 1;
}
static void poison(void) {
 state->poisoned=1;state->flags|=SD_F_POISONED;
 if(controller_live) {
  /* Bounded reset and SDCLK-off only; keep DMA allocation even on failure. */
  SD(0x38)=0;SD(0x34)=0;SD(0x2c)|=1u<<24;
  state->cleanup=wait_reg(0x2030002cu,1u<<24,0,100,SD_RESET_TIMEOUT)?1u:2u;
  SD(0x2c)&=~5u;
  state->clock_control=SD(0x2c);state->controller_state=SD(0x24);
 }
}
static int command(unsigned cmd,uint32_t argument,uint32_t response[4]) {
 uint32_t mode,need=1,start,prev;unsigned i;
 if(state->poisoned) return 0;
 /* Exact read-only command set, no host-supplied command/argument interface. */
 switch(cmd) {
 case 0:mode=0;break;
 case 2:case 9:mode=(1u<<16)|(1u<<19);break;
 case 7:mode=(3u<<16)|CHECKS;break;
 case 8:case 3:case 16:case 55:mode=(2u<<16)|CHECKS;break;
 case 41:mode=2u<<16;break;
 case 17:mode=(2u<<16)|CHECKS|(1u<<21)|0x11u;need=3;break;
 default:return fail(SD_COMMAND_ERROR);
 }
 if(cmd!=17) mode|=1u<<18;
 state->last_cmd=cmd;state->last_arg=argument;
 if(!wait_reg(0x20300024u,(cmd==17||cmd==7)?3u:1u,0,100,SD_INHIBIT_TIMEOUT)) return 0;
 SD(0x38)=0;SD(0x34)=0;SD(0x30)=0xffffffffu;
 SD(0x34)=ERRORS|need;
 if(cmd==17) {
  for(i=0;i<16;++i) dma.before[i]=dma.after[i]=DMA_GUARD;
  for(i=0;i<512;++i) dma.bytes[i]=0xa5;
  SD(4)=512;SD(0)=1;SD(0x58)=(uintptr_t)dma.bytes;SD(0x5c)=0;
 }
 SD(8)=argument;
 __asm__ volatile("dsb sy" ::: "memory");
 SD(0x0c)=cmd<<24|mode;++state->commands;
 start=prev=now();
 for(i=0;i<POLLS;++i) {
  uint32_t status=SD(0x30);state->last_status=status;
  if(status&ERRORS) return fail(SD_COMMAND_ERROR);
  if((status&need)==need) break;
  if(elapsed(start,&prev,1000)) return fail(SD_COMMAND_TIMEOUT);
 }
 if(i==POLLS) return fail(SD_COMMAND_TIMEOUT);
 for(i=0;i<4;++i) response[i]=SD(0x10+i*4);
 state->last_response=response[0];
 if(cmd==7||cmd==16||cmd==17||cmd==55) {
  if(response[0]&R1_ERRORS) return fail(SD_CARD_STATUS_ERROR);
 }
 if(cmd==7||cmd==17) {
  if(!wait_reg(0x20300024u,2,0,1000,SD_INHIBIT_TIMEOUT)) return 0;
 }
 if(cmd==17) {
  __asm__ volatile("dsb sy" ::: "memory");
  for(i=0;i<16;++i) if(dma.before[i]!=DMA_GUARD||dma.after[i]!=DMA_GUARD) return fail(SD_DMA_GUARD);
 }
 return 1;
}
static uint32_t bits(const uint32_t words[4],unsigned low,unsigned count) {
 uint32_t v=words[low>>5]>>(low&31);
 if((low&31)+count>32) v|=words[(low>>5)+1]<<(32-(low&31));
 return v&((1u<<count)-1u);
}
static int capacity(void) {
 /* Standard SDHCI R2 arrangement: raw registers omit CRC/end eight bits.
  * No heuristic retry with another layout. Raw CID/CSD retained in report;
  * actual UMS9117 response alignment still requires physical confirmation.
  */
 uint32_t w[4],structure,csize,shift,count;unsigned i;
 for(i=0;i<4;++i) w[i]=state->csd[i]<<8|(i?state->csd[i-1]>>24:0);
 structure=bits(w,126,2);
 if(!(bits(w,84,12)&4u)) return fail(SD_CSD_UNSUPPORTED);
 if(structure==1&&state->block_addressed) {
  csize=bits(w,48,22);
  if(csize==0x3fffffu) return fail(SD_CSD_UNSUPPORTED); /*2TiB counter wraps*/
  count=(csize+1u)<<10;
 } else if(structure==0&&!state->block_addressed) {
  unsigned len=bits(w,80,4);
  if(len<9||len>11) return fail(SD_CSD_UNSUPPORTED);
  shift=bits(w,47,3)+2+len-9;csize=bits(w,62,12);
  count=(csize+1u)<<shift;
  if(count>0x400000u) return fail(SD_CSD_UNSUPPORTED); /* SDSC <=2GiB. */
 } else return fail(SD_CSD_UNSUPPORTED);
 if(!count) return fail(SD_CSD_UNSUPPORTED);
 state->capacity_lo=count;state->capacity_hi=0;
 return 1;
}
static int initialize(void) {
 uint32_t a,b,r[4],start;unsigned i;
 state->stage=SD_POWER;
 if(!adi_read(0x40608d04u,&state->core_selector)||!adi_read(0x40608d10u,&state->io_selector)) return 0;
 state->core_uv=2000000u+(state->core_selector&127u)*12500u;
 state->io_uv=1613000u+(state->io_selector&127u)*12500u;
 if(state->core_uv<2700000u||state->core_uv>3600000u||state->io_uv<2700000u||state->io_uv>3600000u) return fail(SD_VOLTAGE_UNSUPPORTED);
 /* Advertise only100mV OCR windows containing BOTH inherited rail voltages. */
 for(i=15;i<=23;++i) {
  uint32_t lo=2700000u+(i-15)*100000u;
  if(state->core_uv>=lo&&state->core_uv<=lo+100000u&&state->io_uv>=lo&&state->io_uv<=lo+100000u) state->host_ocr|=1u<<i;
 }
 if(!state->host_ocr) return fail(SD_VOLTAGE_UNSUPPORTED);
 if(!adi_read(0x40608d00u,&a)||!adi_read(0x40608d0cu,&b)||
    !adi_write(0x40608d00u,a&~1u)||!adi_write(0x40608d0cu,b&~1u)) return 0;
 if(!adi_read(0x40608d00u,&a)||!adi_read(0x40608d0cu,&b)) return 0;
 if((a|b)&1u) return fail(SD_POWER_VERIFY);
 state->flags|=SD_F_POWER;
 state->stage=SD_PADS;
 REG(0x402e1000u)=0x100000u;REG(0x402e10b0u)=0x1000u;
 for(i=0;i<12;++i) REG(peaq_sd_pins[i][0])=peaq_sd_pins[i][1];
 /* SDIO0 only. No broad mux write and no other original pad restoration. */
 REG(0x20e01000u)=0x80u;controller_live=1;
 REG(0x20e01004u)=0x800u;
 if(!delay(1)) return 0;
 REG(0x20e02004u)=0x800u;
 state->stage=SD_CLOCK;
 REG(0x2150006cu)=(REG(0x2150006cu)&~7u)|1u; /* original26M_XTL */
 SD(0x2c)|=1u<<24;
 if(!wait_reg(0x2030002cu,1u<<24,0,100,SD_RESET_TIMEOUT)) return 0;
 SD(0x38)=0;SD(0x34)=0;SD(0x28)&=~0x3eu; /*1bit, normal speed, reset DMA mode*/
 SD(0x2c)&=~5u;
 SD(0x2c)=(SD(0x2c)&~0x000fffc0u)|0x00084000u; /*divider64, timeout8*/
 SD(0x2c)|=1;
 if(!wait_reg(0x2030002cu,2,2,100,SD_CLOCK_TIMEOUT)) return 0;
 SD(0x2c)|=4;SD(0x3c)&=~(1u<<27);
 if(!delay(20)) return 0; /*power settling and >=74 initial clocks*/
 state->stage=SD_IDENTIFY;
 if(!command(0,0,r)||!command(8,0x1aa,r)) return 0;
 if(r[0]!=0x1aa) return fail(SD_V2_REQUIRED);
 start=now();
 for(i=0;i<100;++i) {
  if(!command(55,0,r)) return 0;
  if(!(r[0]&32u)) return fail(SD_CARD_STATUS_ERROR);
  if(!command(41,state->host_ocr|0x40000000u,r)) return 0;
  state->card_ocr=r[0];
  if(!(r[0]&state->host_ocr)) return fail(SD_OCR_ERROR);
  if(r[0]&0x80000000u) break;
  if(now()-start>=5000u) return fail(SD_OCR_ERROR);
  if(!delay(20)) return 0;
 }
 if(i==100) return fail(SD_OCR_ERROR);
 state->block_addressed=(r[0]>>30)&1u;
 if(!command(2,0,state->cid)||!command(3,0,r)) return 0;
 if((r[0]&0xe000u)||!(r[0]&0xffff0000u)) return fail(SD_CARD_STATUS_ERROR);
 state->rca=r[0]&0xffff0000u;
 if(!command(9,state->rca,state->csd)||!capacity()||!command(7,state->rca,r)) return 0;
 if(!state->block_addressed&&!command(16,512,r)) return 0;
 state->flags|=SD_F_CARD;state->stage=SD_READY;
 state->clock_control=SD(0x2c);state->controller_state=SD(0x24);return 1;
}
int peaq_sd_init(PeaqSdReport *r) {
 uint32_t start=now();int result;
 state=r;
 if(r->error) return 0;
 if(r->init_count) return fail(SD_ALREADY_RUN);
 if(r->chip_id!=0x98180001u) return fail(SD_CHIP_MISMATCH);
 if(!(r->flags&SD_F_COLD)) return fail(SD_COLD_REQUIRED);
 ++r->init_count;result=initialize();
 if(!result) poison();
 r->elapsed_ms=now()-start;return result;
}
int peaq_sd_read(PeaqSdReport *r,uint32_t lba,uint8_t output[512]) {
 uint32_t response[4];unsigned i;
 state=r;
 if(r->error||r->poisoned) return 0;
 if(!(r->flags&SD_F_CARD)) return fail(SD_NOT_READY);
 if(lba>=r->capacity_lo||(!r->block_addressed&&lba>0x7fffffu)) return fail(SD_SECTOR_RANGE);
 if(r->reads>=32768u) return fail(SD_READ_BUDGET);
 if(!command(17,r->block_addressed?lba:lba<<9,response)) {poison();return 0;}
 for(i=0;i<512;++i) output[i]=dma.bytes[i];
 ++r->reads;return 1;
}
