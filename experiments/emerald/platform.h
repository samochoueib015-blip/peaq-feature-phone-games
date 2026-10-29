#ifndef PEAQ_EMERALD_PLATFORM_H
#define PEAQ_EMERALD_PLATFORM_H
#include <stdint.h>
typedef struct {
    void (*present)(const uint16_t *, uint32_t, uint32_t);
    void (*audio)(const int8_t *, uint32_t, uint32_t);
    void (*save_dirty)(void);
} EmeraldHost;
/* The fixed offline layout and relocatable module share this boundary.
 * The host fills the 128 KiB save mirror before init (0xff for no save). */
int peaq_emerald_init(const EmeraldHost *, uint16_t *gba, uint16_t *portrait,
                      uint8_t *layers);
void peaq_emerald_step(uint32_t gba_buttons);
uint32_t peaq_emerald_frames(void);
uint32_t peaq_emerald_save_changes(void);
uint8_t *peaq_emerald_save_memory(void);
#endif
