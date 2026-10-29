#ifndef PEAQ_EMERALD_GAME_CONTROLS_H
#define PEAQ_EMERALD_GAME_CONTROLS_H
#include <stdint.h>
#include "cold_key_input.h"

uint32_t peaq_emerald_game_key(uint32_t logical_key);
#define PEAQ_EMERALD_VOLUME_MAX 8u
enum {PEAQ_VOLUME_UP=1,PEAQ_VOLUME_DOWN=2};
typedef struct {
 uint32_t magic,level,held,next_repeat,last_ms,changed,error;
} PeaqEmeraldVolume;
/* Input bits are semantic UP/DOWN from a separately verified physical binding.
 * Do not substitute the stock OS's UI message numbers for cold matrix codes.
 * Call from the input poll, including during render waits. One step per press,
 * first repeat after 400 ms, then 125 ms; no burst after a stalled frame.
 * Both buttons cancel each other. Zero is mute. changed persists until saved
 * by the owner; this component performs no settings I/O or hardware changes. */
int peaq_emerald_volume_init(PeaqEmeraldVolume *,uint32_t level,uint32_t now_ms);
int peaq_emerald_volume_poll(PeaqEmeraldVolume *,uint32_t held,uint32_t pressed,uint32_t now_ms);
/* Physically measured on this PEAQ: matrix40/logical0x0a = up,
 * matrix41/logical0x0b = down. Consume only those pending physical press bits;
 * all game/Back state and other key edges remain available to the game tick.
 * Call after acquisition polls, including while waiting for a game deadline. */
int peaq_emerald_volume_input(PeaqEmeraldVolume *,PeaqKeyInput *,uint32_t now_ms);
void peaq_emerald_volume_release(PeaqEmeraldVolume *);
/* Signed 16-bit sample scaling. Full level is unity; zero is exactly silent. */
int16_t peaq_emerald_volume_sample(int16_t sample,uint32_t level);
#endif
