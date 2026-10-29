#ifndef PEAQ_EMERALD_AUDIO_STREAM_H
#define PEAQ_EMERALD_AUDIO_STREAM_H
#include <stdint.h>
#define PEAQ_EMERALD_AUDIO_RING 2048u
#define PEAQ_EMERALD_AUDIO_INPUT_MAX 224u
#define PEAQ_EMERALD_AUDIO_PULL_MAX 1024u
typedef struct {
 uint32_t magic,phase,read_at,write_at,count,input_samples,output_samples;
 uint32_t underflow_samples,overflow_blocks;
 uint32_t history_at;
 int16_t ring[PEAQ_EMERALD_AUDIO_RING];
 int8_t history[16];
} PeaqEmeraldAudio;
/* A software queue, not a codec/DMA driver. Owner serializes all calls.
 * Native signed-8 mono at 13440 Hz -> signed-16 mono at 16000 Hz, ratio 25:21.
 * A 14-tap Q14 windowed-sinc polyphase filter retains fractional phase
 * and 7-input-sample delay across callbacks. The consumer's real 16000 Hz hardware clock paces output;
 * no game-frame or display callback may pretend samples were played.
 */
void peaq_emerald_audio_init(PeaqEmeraldAudio *);
/* Whole block accepted=1; invalid/overflow=0. On overflow, phase and queued
 * samples remain unchanged and overflow_blocks increments. No retained input. */
int peaq_emerald_audio_push(PeaqEmeraldAudio *,const int8_t *,uint32_t count,uint32_t rate);
/* Exactly count output samples on success, missing samples become silence.
 * Volume applies on consumption so changes affect already queued PCM. A DMA
 * adapter must copy to its owned inactive buffer; this is never a DMA pointer.
 * Do not release any memory until the physical stop acknowledges completion. */
int peaq_emerald_audio_pull(PeaqEmeraldAudio *,int16_t *,uint32_t count,uint32_t volume);
#endif
