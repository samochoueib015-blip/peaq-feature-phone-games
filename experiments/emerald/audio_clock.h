#ifndef PEAQ_EMERALD_AUDIO_CLOCK_H
#define PEAQ_EMERALD_AUDIO_CLOCK_H
#include <stdint.h>
typedef struct {
    /* First six words are the stable diagnostic snapshot. */
    uint32_t ticks, resyncs, max_gap_ms, max_burst, last_burst, error;
    uint32_t active, last_ms, due_ms, remainder;
} PeaqAudioClock;
/* Serialized by the owner, outside game callbacks. Long pauses rebase rather
 * than attempting to play all missed audio after a save or pause menu. */
int peaq_audio_clock_advance(PeaqAudioClock *, uint32_t now, void (*tick)(void));
#endif
