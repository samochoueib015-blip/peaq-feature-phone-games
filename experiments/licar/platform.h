#ifndef PEAQ_LICAR_PLATFORM_H
#define PEAQ_LICAR_PLATFORM_H
#include <stdint.h>

/* Engine-facing services; these must be bound to the OS runtime before a
 * phone package can be enabled. RGB565 presentation is portrait 240 x 320. */
typedef struct {
    void (*present)(const uint16_t *, uint32_t, uint32_t);
    void (*yield)(void);
    void (*log)(const char *);
    char (*data_next)(void);
    int (*data_append)(const char *);
    uint32_t (*music_read)(uint8_t *, uint32_t);
} LicarHost;

int peaq_licar_init(const LicarHost *, uint16_t *landscape, uint16_t *portrait);
int peaq_licar_step(uint32_t elapsed_ms, uint32_t buttons);
void peaq_licar_audio(int16_t *, uint32_t samples);
void peaq_licar_end(void);
uint32_t peaq_licar_state(void);
int32_t peaq_licar_speed(void);
uint32_t peaq_licar_time(void);
uint32_t peaq_licar_errors(void);
/* Buttons are Licar's logical UP, RIGHT, DOWN, LEFT, ACCEPT, BACK bits. */
#endif
