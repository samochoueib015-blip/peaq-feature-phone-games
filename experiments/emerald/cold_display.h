#ifndef PEAQ_EMERALD_COLD_DISPLAY_H
#define PEAQ_EMERALD_COLD_DISPLAY_H
#include "frame_diag.h"
#define PEAQ_DISPLAY_SOURCE_START 0x80400000u
#define PEAQ_DISPLAY_SOURCE_END PEAQ_FRAME_STORAGE
enum {PEAQ_DISPLAY_ARGUMENT=-1,PEAQ_DISPLAY_BUSY=-2};
/* Fixed cold ownership only: zeroed BSS, original verified panel profiles,
 * .framebuffer at813d8000 with153728 bytes permanently reserved.
 * MMU/I-cache/D-cache off, SVC IRQ/FIQ masked. Not callable from the stock OS.
 * Init is once per boot. Returns1 success,0 hardware failure,<0 invalid use.
 * Calls serialized; no IRQ calls, callbacks, freeing or restart after poison.
 */
int peaq_cold_display_init(uint32_t sctlr,uint32_t cpsr);
/* Reads exactly153600 native240x320 RGB565 bytes wholly inside the declared
 * source arena. Caller retains source until return. Copies once, then waits
 * for DMA and LCM idle. No second rotation or engine-buffer DMA exposure.
 * On0, stop rendering: fixed buffer stays retained and untouched until reboot.
 * An argument/busy error does not invalidate a previously initialized device.
 */
int peaq_cold_display_present(const uint16_t *,uint32_t width,uint32_t height);
/* Borrowed immutable status for boot lifetime. Existing FRM1 fields reused:
 * frames_target/final_checksum stay0; generation_ms measures source copying;
 * frame_ms is an8-entry ring; counters wrap modulo2^32. No USB protocol here.
 */
const PeaqFrameReport *peaq_cold_display_status(void);
#endif
