#ifndef PEAQ_EMERALD_COLD_AUDIO_H
#define PEAQ_EMERALD_COLD_AUDIO_H
#include <stdint.h>
enum {
 PEAQ_AUDIO_CONTEXT=1,PEAQ_AUDIO_ORDER,PEAQ_AUDIO_GUARD,PEAQ_AUDIO_BUSY,
 PEAQ_AUDIO_ADI,PEAQ_AUDIO_TIMER,PEAQ_AUDIO_ROUTE,PEAQ_AUDIO_DMA,
 PEAQ_AUDIO_TIMEOUT,PEAQ_AUDIO_PAUSE,PEAQ_AUDIO_VOLUME
};
typedef struct {
 uint32_t magic,version,bytes,error,initialized,powered,running,stopped;
 uint32_t channels[2],reserved_mask,owned_mask,completion_mask,poisoned;
 uint32_t submitted,completed,submitted_samples,volume,queued,underflow,overflow;
 uint32_t started_ms,last_ms,dma_started_ms,adi_reads,adi_writes,polls;
 uint32_t drain_blocks,silence_blocks,pause_attempts,pause_acks,guards_ok;
 uint32_t dma_status[2],dma_source[2],stop_complete,vbc_owned;
 uint32_t pcm_peak;
} PeaqColdAudioReport;
/* Serial cold-mode service, never a stock-OS API. The fixed .audio_state
 * section must be linked at0x81140000 and mapped privileged, uncached RW.
 * Start/stop require MMU and caches off. Push/pump also accept the exact owned
 * cache context; AP-DMA/VBC pages must be mapped Device by its caller.
 * Start refuses busy channels/request mappings and takes no OS resources.
 */
int peaq_cold_audio_start(uint32_t volume);
/* Copies signed8 mono13440Hz immediately; no retained engine pointer.
 * Returns1 accepted,0 rejected/full. Overflow is counted, not a fatal error.
 */
int peaq_cold_audio_push(const int8_t *,uint32_t count,uint32_t rate);
/* Call each game step and from existing input/display polling opportunities.
 * Hardware consumes 1600 samples per100ms transaction, in320-byte requests.
 * Both channels must finish and stop before the permanent buffer is refilled.
 * Polling may leave gaps between transactions; this first backend is not a
 * claim of gapless playback. Returns1 healthy,0 failure (buffers stay owned).
 */
int peaq_cold_audio_pump(uint32_t volume,uint32_t now);
/* drain=1 plays queued data and a complete quiet block before muted teardown.
 * drain=0 cancels. Bounded waits, no storage access, no allocation or freeing.
 * Returns1 only when both DMA channels stopped and their mappings released.
 * A timeout permanently poisons this instance; reset is the recovery path.
 */
int peaq_cold_audio_stop(uint32_t drain);
const PeaqColdAudioReport *peaq_cold_audio_status(void);
/* Read-only pre-mute snapshot: seven digital registers, five analog
 * registers, then an ADI response-address mismatch bitmask. Cold context. */
int peaq_cold_audio_route(uint32_t out[13]);
#endif
