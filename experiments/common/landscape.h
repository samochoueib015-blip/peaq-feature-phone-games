#ifndef PEAQ_LANDSCAPE_H
#define PEAQ_LANDSCAPE_H
#include <stdint.h>
/* Hold the physical phone 90 degrees counterclockwise: keypad on the right.
 * Logical (320x240) -> physical portrait (240x320): (239-y, x).
 * The source may be 320x240, 160x120 or GBA 240x160; nearest-neighbour fit
 * preserves aspect ratio with centered letterboxing. Destination is RGB565. */
void peaq_landscape_fit(uint16_t *portrait,const uint16_t *source,int width,int height);
/* D-pad directions after physically rotating the handset. */
enum { PORT_UP=1,PORT_DOWN=2,PORT_LEFT=4,PORT_RIGHT=8,PORT_OK=16,PORT_FIRE=32,PORT_LOOK_LEFT=64,PORT_LOOK_RIGHT=128 };
uint32_t peaq_landscape_dpad(uint32_t portrait_buttons);
#endif
