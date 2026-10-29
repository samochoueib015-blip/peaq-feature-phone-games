/* Bounded, programmed-I/O-only LCD diagnostic for the original PEAQ board.
 * Register bindings: pinned original kernel + pinned FPDoom UMS9117 syscode.
 * No LCDC/DMA, SD, NAND, frequency/voltage changes, or active-OS callbacks.
 * Exact board pads/init sequence are generated privately by the offline build.
 */
#include "lcd_diag.h"
#include "peaq_lcd_board_profile.h"
#include "peaq_gc9306_profile.h"
#include <stddef.h>

#define REG(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define HALF(a) (*(volatile uint16_t *)(uintptr_t)(a))
static PeaqLcdReport *state;
static uint32_t began;
static uint32_t now(void) { return REG(0x4023000cu); }

static int fail(unsigned error) {
    if (!state->error) state->error = error;
    return 0;
}
/* Time and iteration limits are independent: a stalled timer cannot hang us. */
static int delay(unsigned milliseconds) {
    uint32_t start = now(), last = start, stalled = 0, polls;
    /* Independent overall ceiling also covers a changing but broken timer.
     * 64M MMIO reads leave ample headroom for the longest fixed120ms delay;
     * elapsed-time polling remains the normal exit, never an iteration delay.
     */
    for (polls=0; polls<64000000u; ++polls) {
        uint32_t t = now();
        ++state->total_polls;
        if ((t-last)&0x80000000u) return fail(LCD_TIMER_STUCK);
        if (t-start >= milliseconds) return 1;
        if (t != last) { last=t; stalled=0; }
        else if (++stalled >= PEAQ_LCD_POLL_MAX) return fail(LCD_TIMER_STUCK);
    }
    return fail(LCD_TIMER_STUCK);
}
static int bus_ready(void) {
    unsigned i;
    uint32_t start = now();
    for (i=0; i<PEAQ_LCD_POLL_MAX; ++i) {
        uint32_t v = REG(0x20a00008u); /* Original driver, not upstream CR0. */
        state->last_busy=v;
        ++state->total_polls;
        if (!(v&2u)) return 1;
        if (now()-start >= 20u) return fail(LCD_BUS_TIMEOUT);
    }
    return fail(LCD_BUS_TIMEOUT);
}
static void gpio(unsigned id, unsigned offset, unsigned high) {
    uint32_t address=0x40280000u+((id>>4)<<7)+offset, bit=1u<<(id&15);
    uint32_t v=REG(address);
    REG(address)=high ? v|bit : v&~bit;
}
static void select_main(void) { gpio(82,0,1); gpio(40,0,0); }
static void deselect_main(void) { gpio(40,0,1); }
static int transfer(unsigned data, unsigned value) {
    if (!bus_ready()) return 0;
    REG(0x20a00010u)=1;
    select_main();
    if (!bus_ready()) { deselect_main(); return 0; }
    HALF(data ? 0x60020000u : 0x60000000u)=(uint16_t)value;
    __asm__ volatile("dsb sy" ::: "memory");
    deselect_main();
    ++state->bus_writes;
    return 1;
}
static int command(unsigned c) { return transfer(0,c); }
static int data(unsigned c) { return transfer(1,c); }
static int reset_panel(void) {
    unsigned i;
    state->stage=LCD_STAGE_RESET;
    for (i=0; i<3; ++i) {
        REG(i==1 ? 0x402e260cu : 0x402e160cu)=1;
        if (!delay(16)) return 0;
    }
    return delay(120);
}

static void finish(void) {
    state->elapsed_ms=now()-began;
    state->gpio40=REG(0x40280100u);
    state->gpio82=REG(0x40280280u);
    state->lcm_control=REG(0x20a00000u);
    state->lcm_mode=REG(0x20a00010u);
    state->lcm_timing=REG(0x20a00014u);
}

int peaq_lcd_identify(PeaqLcdReport *r) {
    unsigned i;
    state=r; began=now();
    if (r->error) return 0;
    if (r->identify_count) return fail(LCD_ALREADY_RUN);
    if (r->chip_id != 0x98180001u) return fail(LCD_CHIP_MISMATCH);
    if (!(r->flags&LCD_F_COLD)) return fail(LCD_COLD_REQUIRED);
    ++r->identify_count;
    r->stage=LCD_STAGE_PADS;
    /* FPDoom cold pin_init, restricted to this phone's digital pad entries.
     * This reinstates ALL original digital pads, including inactive peripherals;
     * no commands are sent to those peripherals. Analogue pads are untouched.
     */
    REG(0x402e1000u)=0x100000u;
    REG(0x402e10b0u)=0x1000u;
    peaq_shared_pins_init();
    REG(0x402a0018u)&=0xe0000000u;
    r->stage=LCD_STAGE_GPIO;
    REG(0x402e1000u)=8u;
    /* Original GPIO mask/output/input-enable semantics; avoid selecting either
     * panel while the directions change. No other GPIO state is initialized.
     */
    gpio(40,4,1); gpio(40,0,1); gpio(40,8,1); gpio(40,0x28,0);
    gpio(82,4,1); gpio(82,0,1); gpio(82,8,1); gpio(82,0x28,0);
    r->stage=LCD_STAGE_BUS;
    REG(0x20e01000u)=0x1000u;
    REG(0x20a00000u)=0;
    REG(0x20a00010u)=1;
    REG(0x20a00014u)=0x00a50100u;
    /* Upstream worst-case timing fields; no guessed AHB clock/divider change. */
    REG(0x20a00014u)=(30u<<16)|6u|0x80u|(30u<<21)|(14u<<8)|(6u<<4);
    if (!reset_panel()) { finish(); return 0; }
    r->stage=LCD_STAGE_IDENTIFY;
    if (!command(4)) { finish(); return 0; }
    select_main();
    for (i=0; i<4; ++i) {
        if (!bus_ready()) { deselect_main(); finish(); return 0; }
        REG(0x20a00010u)=1;
        r->id_bytes=(r->id_bytes<<8)|(HALF(0x60020000u)&255u);
    }
    deselect_main();
    r->panel_id=r->id_bytes&0xffffu;
    if (r->panel_id != 0x9306u) fail(LCD_PANEL_MISMATCH);
    else r->flags|=LCD_F_IDENTIFIED;
    finish();
    return r->error==LCD_OK;
}

static int init_sequence(void) {
    unsigned offset=0;
    while (offset<sizeof(peaq_gc9306_init)) {
        unsigned tag=peaq_gc9306_init[offset++], n=tag&31u, i;
        if (!tag) return offset==sizeof(peaq_gc9306_init) ? 1 : fail(LCD_PROFILE_INVALID);
        if ((tag&0xe0u)==0x80u) {
            if (offset+n+1>sizeof(peaq_gc9306_init)) return fail(LCD_PROFILE_INVALID);
            if (!command(peaq_gc9306_init[offset++])) return 0;
            for (i=0;i<n;++i) if (!data(peaq_gc9306_init[offset++])) return 0;
        } else if ((tag&0xe0u)==0x40u) {
            if (offset>=sizeof(peaq_gc9306_init)) return fail(LCD_PROFILE_INVALID);
            if (!delay((n<<8)|peaq_gc9306_init[offset++])) return 0;
        } else return fail(LCD_PROFILE_INVALID);
    }
    return fail(LCD_PROFILE_INVALID);
}

static int adi_read(uint32_t address, uint32_t *value) {
    unsigned i;
    uint32_t start=now();
    REG(0x40600028u)=address;
    for(i=0;i<4096;++i) {
        uint32_t v=REG(0x4060002cu);
        ++state->total_polls;
        if (!(v&0x80000000u)) { *value=v&0xffffu; return 1; }
        if(now()-start>=10u) break;
    }
    return fail(LCD_ADI_TIMEOUT);
}
static int adi_write(uint32_t address, uint32_t value) {
    unsigned i;
    uint32_t start=now();
    for(i=0;i<4096;++i) {
        ++state->total_polls;
        if (!(REG(0x40600030u)&0x800u)) { REG(address)=value&0xffffu; return 1; }
        if(now()-start>=10u) break;
    }
    return fail(LCD_ADI_TIMEOUT);
}
static int adi_bits(uint32_t address,uint32_t clear,uint32_t set) {
    uint32_t v;
    return adi_read(address,&v) && adi_write(address,(v&~clear)|set);
}
static int light_on(void) {
    unsigned i;
    /* Original PEAQ LED setup, excluding flash-LED clock/value changes.
     * Original brightness channel 1 uses four PWM channels, current0x22.
     * Fixed20% duty: 10 of50, deliberately no arbitrary brightness command.
     */
    state->stage=LCD_STAGE_BACKLIGHT;
    if(!adi_bits(0x40608c08u,0,0x200u) || !adi_bits(0x40608c10u,0,0x80u) ||
       !adi_bits(0x40608decu,5,0) || !adi_bits(0x406081c8u,255,0)) return 0;
    for(i=0;i<4;++i) if(!adi_bits(0x406081b8u+4*i,63,0x22)) return 0;
    for(i=0;i<3;++i) if(!adi_write(0x40608188u+16*i,0x0a32u)) return 0;
    if(!adi_write(0x406081ccu,0x0a32u) || !adi_bits(0x40608180u,0,0x3333u)) return 0;
    state->backlight_percent=20;
    state->flags|=LCD_F_LIT;
    return 1;
}

/* Native portrait storage, with an asymmetric marker for orientation checking.
 * RGB565 white border, upper left yellow marker, red/green/blue bands.
 * No external image, framebuffer pointer, or device DMA is accepted.
 */
static unsigned pixel(unsigned x,unsigned y) {
    if(x<4 || x>=236 || y<4 || y>=316) return 0xffff;
    if(x>=12 && x<44 && y>=12 && y<44) return 0xffe0;
    if(y>=288 && x>=16 && x<224) return (x&16) ? 0xffff : 0;
    return x<80 ? 0xf800 : x<160 ? 0x07e0 : 0x001f;
}
int peaq_lcd_draw(PeaqLcdReport *r) {
    unsigned x,y;
    state=r; began=now();
    if(r->error || !(r->flags&LCD_F_IDENTIFIED)) return fail(LCD_NOT_IDENTIFIED);
    if(r->draw_count) return fail(LCD_ALREADY_RUN);
    ++r->draw_count;
    if(!reset_panel()) { finish(); return 0; }
    r->stage=LCD_STAGE_INIT;
    if(!init_sequence()) { finish(); return 0; }
    if(!command(0x2a)||!data(0)||!data(0)||!data(0)||!data(239)||
       !command(0x2b)||!data(0)||!data(0)||!data(1)||!data(63)||
       !command(0x2c)) { finish(); return 0; }
    r->stage=LCD_STAGE_FRAME;
    for(y=0;y<320;++y) {
        if(now()-began>=10000u) { fail(LCD_BUS_TIMEOUT); finish(); return 0; }
        for(x=0;x<240;++x) {
            unsigned v=pixel(x,y);
            if(!data(v>>8)||!data(v&255)) { finish(); return 0; }
            ++r->pixel_count;
        }
    }
    if(!bus_ready()) { finish(); return 0; }
    r->flags|=LCD_F_DRAWN;
    if(!light_on()) { finish(); return 0; }
    r->stage=LCD_STAGE_COMPLETE;
    finish();
    return 1;
}

int peaq_lcd_dark(PeaqLcdReport *r) {
    state=r; began=now();
    if(!(r->flags&LCD_F_DRAWN) || r->dark_count) return fail(LCD_NOT_IDENTIFIED);
    ++r->dark_count;
    r->stage=LCD_STAGE_DARK;
    if(!adi_bits(0x40608180u,0x1111u,0)) { finish(); return 0; }
    r->flags=(r->flags&~LCD_F_LIT)|LCD_F_DARK;
    r->backlight_percent=0;
    finish();
    return 1;
}

#include "brightness_lcd_v21.inc"
