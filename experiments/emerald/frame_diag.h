#ifndef PEAQ_EMERALD_FRAME_DIAG_H
#define PEAQ_EMERALD_FRAME_DIAG_H
#include <stdint.h>
#define PEAQ_FRAME_INFO 0x7400u
#define PEAQ_FRAME_INIT 0x7401u
#define PEAQ_FRAME_RUN 0x7402u
#define PEAQ_FRAME_MAGIC 0x314d5246u /* FRM1 */
#define PEAQ_FRAME_COUNT 8u
#define PEAQ_FRAME_STORAGE 0x813d8000u
#define PEAQ_FRAME_BUFFER (PEAQ_FRAME_STORAGE+64u)
#define PEAQ_FRAME_BYTES 153600u
enum { FRAME_OK,FRAME_BASELINE_ERROR,FRAME_ORDER,FRAME_ALREADY_RUN,
 FRAME_TIMER_ERROR,FRAME_BUS_TIMEOUT,FRAME_IRQ_STALE,FRAME_DMA_TIMEOUT,
 FRAME_GUARD_ERROR,FRAME_POISONED };
enum { FRAME_IDLE,FRAME_BASELINE,FRAME_SETUP,FRAME_READY,FRAME_GENERATE,
 FRAME_TRANSFER,FRAME_COMPLETE,FRAME_FAILED };
enum { FRAME_F_COLD=1,FRAME_F_BASELINE=2,FRAME_F_LCDC=4,FRAME_F_COMPLETE=8,FRAME_F_POISONED=16 };
typedef struct {
 uint32_t magic,version,bytes,flags;
 uint32_t chip_id,entered_sctlr,entered_cpsr,timer_ms;
 uint32_t stage,error,init_count,run_count;
 uint32_t frames_done,frames_target,pixels_sent,polls;
 uint32_t panel_id,baseline_error,baseline_flags,baseline_ms;
 uint32_t lcdc_control,image_control,irq_raw,fifo_status;
 uint32_t lcm_mode,lcm_timing,framebuffer,frame_bytes;
 uint32_t generation_ms,transfer_ms,total_ms,last_busy;
 uint32_t guards_ok,poisoned,profile_crc32,final_checksum;
 uint32_t frame_ms[8];
 uint32_t setup_ms,frame_commands,backlight_percent,reserved;
} PeaqFrameReport;
int peaq_frame_initialize(void);
int peaq_frame_run(void);
int peaq_frame_dispatch(unsigned type,unsigned length);
#endif
