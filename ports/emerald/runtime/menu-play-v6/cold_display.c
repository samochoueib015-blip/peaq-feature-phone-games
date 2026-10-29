#include "menu_loading.h"
/* Cold-only reusable display service. LCDC setup/transfer/poison operations
 * are retained from the physically qualified eight-frame diagnostic.
 * Engine pixels are copied to one permanently owned, fixed DMA buffer.
 * No allocator, USB commands, engine execution, SD/NAND or OS callback.
 */
#include "cold_display.h"
#include "lcd_diag.h"
#include "peaq_lcd_board_profile.h"
#define REG(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define LCD(off) REG(0x20800000u+(off))
#define GUARD 0xb6932a17u
static PeaqFrameReport report;
static PeaqLcdReport baseline;
static unsigned lcdc_live;
static struct {uint32_t before[16];uint16_t pixels[240*320];uint32_t after[16];}
 frame __attribute__((section(".framebuffer"),aligned(64)));
_Static_assert(sizeof(PeaqFrameReport)==192,"FRM1 layout changed");
static uint32_t sys_timer_ms(void) {return REG(0x4023000cu);}
static int fail(unsigned e) {if(!report.error)report.error=e;return 0;}
static int tick(uint32_t start,uint32_t *prev,unsigned milliseconds) {
 uint32_t t=sys_timer_ms();++report.polls;
 if((t-*prev)&0x80000000u) {fail(FRAME_TIMER_ERROR);return -1;}
 *prev=t;return t-start>=milliseconds;
}
static int wait_mask(uint32_t address,uint32_t mask,uint32_t expected,unsigned ms,unsigned error) {
 unsigned i;uint32_t start=sys_timer_ms(),prev=start;
 for(i=0;i<1000000u;++i) {
  uint32_t v=REG(address);report.last_busy=v;
  if((v&mask)==expected)return 1;
  if(tick(start,&prev,ms))break;
 }
 return fail(error);
}
static int delay(unsigned ms) {
 unsigned i,stalled=0;uint32_t start=sys_timer_ms(),prev=start,last=start;
 for(i=0;i<64000000u;++i) {
  int e=tick(start,&prev,ms);if(e)return e>0;
  if(prev==last) {if(++stalled==1000000u)return fail(FRAME_TIMER_ERROR);}
  else {stalled=0;last=prev;}
 }
 return fail(FRAME_TIMER_ERROR);
}
static int bus_ready(void) {return wait_mask(0x20a00008u,2,0,20,FRAME_BUS_TIMEOUT);}
static void gpio40(unsigned high) {
 uint32_t v=REG(0x40280100u);REG(0x40280100u)=high?v|0x100u:v&~0x100u;
}
static void select_main(void) {REG(0x40280280u)|=4u;gpio40(0);}
static void snapshot(void) {
 if(lcdc_live) {
  report.lcdc_control=LCD(0);report.image_control=LCD(0x20);
  report.irq_raw=LCD(0x11c);report.fifo_status=LCD(0x14);
 }
 if(report.flags&FRAME_F_BASELINE) {report.lcm_mode=REG(0x20a00010u);report.lcm_timing=REG(0x20a00014u);}
}
static void poison(void) {
 report.poisoned=1;report.flags|=FRAME_F_POISONED;report.stage=FRAME_FAILED;
 if(lcdc_live) {
  LCD(0x110)=0;LCD(0x20)&=~1u;LCD(0)&=~9u;
  /* Leave LCDC in asserted reset. No assumption that a bad DMA acknowledges
   * abort; the framebuffer remains allocated and unchanged for image lifetime.
   */
  REG(0x20e01004u)=2;
  if(bus_ready()) {gpio40(1);REG(0x20a00010u)=1;}
 }
 snapshot();
}
static int guards(void) {
 unsigned i;
 report.guards_ok=0;
 for(i=0;i<16;++i)if(frame.before[i]!=GUARD||frame.after[i]!=GUARD)return fail(FRAME_GUARD_ERROR);
 report.guards_ok=1;return 1;
}
static int initialize(void) {
 uint32_t start=sys_timer_ms();unsigned i;
 if(report.error)return 0;
 if(report.init_count)return fail(FRAME_ALREADY_RUN);
 ++report.init_count;report.stage=FRAME_BASELINE;
 if(!peaq_lcd_identify(&baseline)||!peaq_lcd_draw(&baseline)) {
  report.baseline_error=baseline.error;report.baseline_flags=baseline.flags;
  report.panel_id=baseline.panel_id;return fail(FRAME_BASELINE_ERROR);
 }
 report.baseline_ms=sys_timer_ms()-start;report.panel_id=baseline.panel_id;
 report.baseline_error=baseline.error;report.baseline_flags=baseline.flags;
 report.backlight_percent=baseline.backlight_percent;report.flags|=FRAME_F_BASELINE;
 report.stage=FRAME_SETUP;start=sys_timer_ms();
 for(i=0;i<16;++i)frame.before[i]=frame.after[i]=GUARD;
 /* Own reset means no enabled legacy layers or unknown DMA addresses survive.
  * Exact clock/reset bindings are executed in the original-code source test.
  */
 REG(0x20e01000u)=0x800u;lcdc_live=1;REG(0x20e01004u)=2;
 if(!delay(10)) {poison();return 0;}
 REG(0x20e02004u)=2;
 LCD(0)=3; /*enable, FMark mode1/polarity0: pinned upstream setting*/
 LCD(0x110)=0;LCD(0x114)=1;
 LCD(0x20)=0;LCD(0x50)=0;LCD(0x80)=0;LCD(0xb0)=0;
 LCD(0x10)=0;LCD(4)=240u|320u<<16;LCD(8)=0;LCD(12)=240u|320u<<16;
 LCD(0xe0)=0xa0;LCD(0xe4)=0x60020000u>>2;
 LCD(0x24)=(uintptr_t)frame.pixels>>2;LCD(0x28)=0;
 LCD(0x2c)=240u|320u<<16;LCD(0x30)=240;LCD(0x34)=0;
 LCD(0x20)=0x251; /*RGB565, little-endian word order, IMG enabled*/
 if(!bus_ready()||!guards()) {poison();return 0;}
 REG(0x20a00010u)=0x28;
 report.flags|=FRAME_F_LCDC;report.stage=FRAME_READY;
 report.setup_ms=sys_timer_ms()-start;snapshot();return 1;
}
static int transfer(void) {
 if(!bus_ready())return 0;
 /* One exact Memory Write per frame. Hold the already-qualified main-panel
  * CS throughout transfer; the outer panel stays deselected. No repeated reset.
  */
 REG(0x20a00010u)=1;select_main();
 *(volatile uint16_t*)(uintptr_t)0x60000000u=0x2c;
 __asm__ volatile("dsb sy" ::: "memory");
 if(!bus_ready())return 0;
 REG(0x20a00010u)=0x28;
 LCD(0x114)=1;
 if(!wait_mask(0x2080011cu,1,0,20,FRAME_IRQ_STALE))return 0;
 __asm__ volatile("dsb sy" ::: "memory");
 LCD(0x110)=1;LCD(0)|=8u;++report.frame_commands;
 if(!wait_mask(0x2080011cu,1,1,1000,FRAME_DMA_TIMEOUT)||!bus_ready())return 0;
 LCD(0x114)=1;LCD(0x110)=0;
 __asm__ volatile("dsb sy" ::: "memory");
 gpio40(1);
 return guards();
}

static unsigned entered,active;
const PeaqFrameReport *peaq_cold_display_status(void) {return &report;}
int peaq_cold_display_init(uint32_t sctlr,uint32_t cpsr) {
 if(active||entered)return PEAQ_DISPLAY_BUSY;
 if((sctlr&0x1005u)||(cpsr&0x1ffu)!=0x1d3u||
    (uintptr_t)&frame!=PEAQ_FRAME_STORAGE)return PEAQ_DISPLAY_ARGUMENT;
 entered=active=1;
 report.magic=PEAQ_FRAME_MAGIC;report.version=1;report.bytes=sizeof(report);
 report.flags=FRAME_F_COLD;report.entered_sctlr=sctlr;report.entered_cpsr=cpsr;
 report.chip_id=REG(0x402e00fcu);report.profile_crc32=PEAQ_LCD_PROFILE_CRC32;
 report.framebuffer=(uintptr_t)frame.pixels;report.frame_bytes=sizeof(frame.pixels);
 baseline.flags=LCD_F_COLD|LCD_F_BOARD;baseline.chip_id=report.chip_id;
 baseline.entered_sctlr=sctlr;baseline.entered_cpsr=cpsr;
 baseline.profile_crc32=PEAQ_LCD_PROFILE_CRC32;baseline.digital_pin_count=PEAQ_LCD_DIGITAL_PINS;
 int result=initialize();active=0;return result;
}
int peaq_cold_display_present(const uint16_t *pixels,uint32_t width,uint32_t height) {
 uintptr_t src=(uintptr_t)pixels;
 if(active)return PEAQ_DISPLAY_BUSY;
 if(width!=240||height!=320||(src&1u)||src<PEAQ_DISPLAY_SOURCE_START||
    src>PEAQ_DISPLAY_SOURCE_END-PEAQ_FRAME_BYTES)return PEAQ_DISPLAY_ARGUMENT;
 if(!entered||report.error||report.poisoned||!(report.flags&FRAME_F_LCDC))return 0;
 active=1;uint32_t start=sys_timer_ms(),copy=start;
 if(!guards()){poison();active=0;return 0;}
 report.stage=FRAME_GENERATE;
 /* Fixed native geometry. The runtime already rotates for90-degreeCCW use.
  * No write to a caller buffer and no DMA from caller-owned memory. */
 for(unsigned i=0;i<240u*320u;++i)frame.pixels[i]=pixels[i];
 peaq_menu_volume_overlay(frame.pixels,sys_timer_ms());
 report.generation_ms+=sys_timer_ms()-copy;
 report.stage=FRAME_TRANSFER;uint32_t began=sys_timer_ms();
 if(!transfer()){poison();active=0;return 0;}
 uint32_t elapsed=sys_timer_ms()-began;
 report.frame_ms[report.frames_done&7u]=elapsed;report.transfer_ms+=elapsed;
 ++report.frames_done;++report.run_count;report.pixels_sent+=240u*320u;
 report.total_ms+=sys_timer_ms()-start;report.flags|=FRAME_F_COMPLETE;
 report.stage=FRAME_COMPLETE;report.timer_ms=sys_timer_ms();snapshot();
 active=0;return 1;
}
