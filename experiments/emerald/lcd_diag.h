#ifndef PEAQ_EMERALD_LCD_DIAG_H
#define PEAQ_EMERALD_LCD_DIAG_H
#include <stdint.h>

/* New, separate cold RAM diagnostic. Not callable from an OS game. */
#define PEAQ_LCD_INFO 0x7200u
#define PEAQ_LCD_IDENTIFY 0x7201u
#define PEAQ_LCD_DRAW 0x7202u
#define PEAQ_LCD_DARK 0x7203u
#define PEAQ_LCD_REPORT_MAGIC 0x3144434cu /* LCD1 */
#define PEAQ_LCD_POLL_MAX 1000000u

enum {
    LCD_OK, LCD_COLD_REQUIRED, LCD_CHIP_MISMATCH, LCD_TIMER_STUCK,
    LCD_BUS_TIMEOUT, LCD_ADI_TIMEOUT, LCD_PANEL_MISMATCH,
    LCD_ALREADY_RUN, LCD_NOT_IDENTIFIED, LCD_PROFILE_INVALID
};
enum {
    LCD_STAGE_IDLE, LCD_STAGE_PADS, LCD_STAGE_GPIO, LCD_STAGE_BUS,
    LCD_STAGE_RESET, LCD_STAGE_IDENTIFY, LCD_STAGE_INIT,
    LCD_STAGE_FRAME, LCD_STAGE_BACKLIGHT, LCD_STAGE_COMPLETE,
    LCD_STAGE_DARK
};
enum {
    LCD_F_COLD=1, LCD_F_BOARD=2, LCD_F_IDENTIFIED=4,
    LCD_F_DRAWN=8, LCD_F_LIT=16, LCD_F_DARK=32
};
/* 32 fixed little-endian words, no arbitrary-address query. */
typedef struct {
    uint32_t magic, version, bytes, flags;
    uint32_t chip_id, entered_sctlr, entered_cpsr, timer_ms;
    uint32_t stage, error, panel_id, id_bytes;
    uint32_t identify_count, draw_count, dark_count, bus_writes;
    uint32_t pixel_count, last_busy, total_polls, elapsed_ms;
    uint32_t profile_crc32, digital_pin_count, gpio40, gpio82;
    uint32_t lcm_control, lcm_mode, lcm_timing, backlight_percent;
    uint32_t image_end, bss_end, reserved0, reserved1;
} PeaqLcdReport;

int peaq_lcd_identify(PeaqLcdReport *r);
int peaq_lcd_draw(PeaqLcdReport *r);
int peaq_lcd_dark(PeaqLcdReport *r);
int peaq_lcd_dispatch(unsigned type, unsigned length);
#endif
