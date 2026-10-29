#ifndef PEAQ_MENU_VIDEO_V25_H
#define PEAQ_MENU_VIDEO_V25_H
#include <stdint.h>
enum {MENU_VIDEO_FULL=1,MENU_VIDEO_FPS=2,MENU_VIDEO_BATTERY=4,MENU_VIDEO_MASK=7};
void peaq_menu_video_bind(const uint16_t *portrait,const uint16_t *gba);
unsigned peaq_menu_video_flags(void);
void peaq_menu_video_set(unsigned flags);
void peaq_menu_video_reset_clock(void);
void peaq_menu_video_battery(unsigned millivolts);
void peaq_menu_video_copy(uint16_t *out,const uint16_t *source,uint32_t now);
unsigned peaq_menu_video_fps(void);
/* EPR1 revision16 reserves words21..25 for preferences/overlay telemetry.
 * The existing whole-record checksum protects it; no extra writable sector. */
void peaq_menu_video_restore(const uint32_t report[128]);
void peaq_menu_video_record(uint32_t report[128]);
#endif
