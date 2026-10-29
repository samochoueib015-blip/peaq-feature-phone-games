#ifndef PEAQ_BRIGHTNESS_V21_H
#define PEAQ_BRIGHTNESS_V21_H
#include "game_controls.h"
/* M1/M3 share the extra-key matrix row, columns1/2. The level is never
 * allowed to reach zero so the LCD cannot become unusably dark. */
int peaq_brightness_input(PeaqEmeraldVolume *,PeaqKeyInput *,uint32_t);
int peaq_lcd_brightness(unsigned);
void peaq_menu_brightness_notice(unsigned,uint32_t);
#endif
