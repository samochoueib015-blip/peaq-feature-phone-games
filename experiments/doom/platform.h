#ifndef PEAQ_DOOM_PLATFORM_H
#define PEAQ_DOOM_PLATFORM_H
#include <stdint.h>
#include "../common/landscape.h"
enum { PORT_USE=256,PORT_MENU=512,PORT_ACCEPT=1024 };
typedef struct {
 uint32_t (*milliseconds)(void);
 void (*sleep_ms)(uint32_t);
 void (*present)(const uint16_t *portrait,int width,int height);
} DoomHost;
/* Host and buffer must outlive the engine. No firmware addresses are used.
 * The caller supplies actual monotonic time and a scheduler-backed sleep.
 * This bridge alone does not make DoomGeneric cooperative or allocate its
 * heap/files/audio. Those services still need the next phone runtime. */
int peaq_doom_bind(const DoomHost *host,uint16_t *portrait_240x320);
void peaq_doom_buttons(uint32_t logical_buttons);
uint32_t peaq_doom_overflows(void);
#endif
