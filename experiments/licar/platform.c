/* Full upstream Licar, compiled unchanged behind a small OS-facing adapter. */
#include "platform.h"
#define LCR_SETTING_RESOLUTION_X 320
#define LCR_SETTING_RESOLUTION_Y 240
#define LCR_SETTING_FPS 20
#define LCR_SETTING_LOG_LEVEL 0
#define LCR_LOADING_COMMAND do { if (host->yield) host->yield(); } while (0);
static const LicarHost *host;
static uint16_t *screen, *portrait;
static uint32_t held, clock_ms, errors;
static uint8_t active;
#include "game.h"
#include "../common/landscape.h"

uint8_t LCR_keyPressed(uint8_t key) { return (held >> key) & 1; }
void LCR_sleep(uint16_t ms) {
    /* One step returns to the caller; the OS timer owns the wait. Loading can
     * yield through the explicit callback when run on an OS worker. */
    (void)ms;
}
void LCR_drawPixel(unsigned long i, uint16_t color) {
    if (i < 320u * 240u) screen[i] = color;
    else errors |= 1;
}
void LCR_log(const char *s) { if (host->log) host->log(s); }
char LCR_getNextDataFileChar(void) {
    return host->data_next ? host->data_next() : 0;
}
void LCR_appendDataStr(const char *s) {
    if (!host->data_append || !host->data_append(s)) errors |= 2;
}
int peaq_licar_init(const LicarHost *h, uint16_t *land, uint16_t *port) {
    if (!h || !h->present || !land || !port) return 0;
    host = h; screen = land; portrait = port;
    held = clock_ms = errors = 0;
    for (unsigned i = 0; i < 76800; i++) screen[i] = portrait[i] = 0;
    LCR_gameInit(0, 0);
    active = 1;
    return 1;
}
int peaq_licar_step(uint32_t elapsed_ms, uint32_t buttons) {
    if (!active) return 0;
    /* Focus loss must pause the clock, not simulate minutes of catch-up. */
    if (elapsed_ms > 100) elapsed_ms = 100;
    clock_ms += elapsed_ms;
    held = buttons & 63;
    int result = LCR_gameStep(clock_ms);
    peaq_landscape_fit(portrait, screen, 320, 240);
    host->present(portrait, 240, 320);
    return result;
}
void peaq_licar_audio(int16_t *out, uint32_t samples) {
    uint8_t music[128];
    while (samples) {
        uint32_t n = samples > sizeof(music) ? sizeof(music) : samples;
        for (uint32_t i = 0; i < n; i++) music[i] = 128;
        if (active && LCR_gameMusicOn() && host->music_read) {
            uint32_t got = host->music_read(music, n);
            if (got > n) { errors |= 4; got = n; }
            for (uint32_t i = got; i < n; i++) music[i] = 128;
        }
        for (uint32_t i = 0; i < n; i++) {
            int sfx = active ? LCR_gameGetNextAudioSample() : 128;
            out[i] = (int16_t)(((int)music[i] + sfx - 256) * 128);
        }
        samples -= n; out += n;
    }
}
void peaq_licar_end(void) {
    if (active) LCR_gameEnd();
    active = 0; held = 0;
}
uint32_t peaq_licar_state(void) { return LCR_game.state; }
int32_t peaq_licar_speed(void) { return LCR_racingGetCarSpeedSigned(); }
uint32_t peaq_licar_time(void) { return LCR_game.runTime; }
uint32_t peaq_licar_errors(void) { return errors; }
