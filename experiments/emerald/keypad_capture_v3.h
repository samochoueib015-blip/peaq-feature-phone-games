#ifndef PEAQ_KEYPAD_CAPTURE_V3_H
#define PEAQ_KEYPAD_CAPTURE_V3_H
#include <stdint.h>
#define KEY_DIAG_INFO 0x7b00u
#define KEY_DIAG_INIT 0x7b01u
#define KEY_DIAG_WINDOW 0x7b02u
#define KEY_DIAG_STOP 0x7b03u
#define KEY_DIAG_INPUT 0x7b04u
#define KEY_DIAG_RECORD0 0x7b20u
#define KEY_DIAG_RECORDS 64u
#define KEY_DIAG_WINDOWS 256u
#define KEY_DIAG_WINDOW_MS 250u
#define KEY_DIAG_MAX_POLLS 64000000u
#define KEY_DIAG_STALLED_POLLS 1000000u
#define KEY_DIAG_MAGIC 0x3344504bu /* KPD3 */
#define KEY_DIAG_RECORD_MAGIC 0x3352504bu /* KPR3 */
enum {KPD_F_COLD=1,KPD_F_BOARD=2,KPD_F_INIT=4,KPD_F_EVENT=8,KPD_F_STOP=16,KPD_F_ERROR=32};
enum {KPD_STAGE_IDLE,KPD_STAGE_PADS,KPD_STAGE_RESET,KPD_STAGE_READY,KPD_STAGE_CAPTURE,KPD_STAGE_STOP,KPD_STAGE_FAILED};
enum {KPD_OK,KPD_COLD_REQUIRED,KPD_CHIP_MISMATCH,KPD_CONTROL,KPD_BAD_RAW,KPD_BAD_CLEAR,
 KPD_TORN_SLOT,KPD_UNMAPPED,KPD_LOG_FULL,KPD_TIMER,KPD_GUARD,KPD_MATRIX};
enum {KPD_PHASE_NONE,KPD_PHASE_PRECONDITIONS,KPD_PHASE_PADS,KPD_PHASE_RESET,KPD_PHASE_RESET_CHECK,KPD_PHASE_FIRST_CTRL,KPD_PHASE_MASK_OR,KPD_PHASE_AUX_CONFIG,KPD_PHASE_ENABLE_CHECK,KPD_PHASE_CAPTURE,KPD_PHASE_STOP,KPD_PHASE_CONFIG_READBACK};
/* Payloads are fixed <=208 bytes; worst-case escaped frames must fit512. */
typedef struct {
 uint32_t magic,version,bytes,flags,chip_id,sctlr,cpsr,current_cpsr;
 uint32_t stage,error,init_count,window_count,window_limit,window_ms,poll_count,capture_ms;
 uint32_t record_count,record_limit,ack_count,max_held,last_raw,last_clear,last_status,control;
 uint32_t divider,debounce,polarity,interrupt_enable,pin_count,pin_crc,key_crc,key_entries;
 uint32_t held[2],gba,host,presses,releases,longs,suppressed,coalesced,invalid,stopped,guard_ok;
 uint32_t image_end,bss_end,timer_ms,reserved;
 uint32_t reset_control,configured_control,error_phase,first_control_write;
} PeaqKeyDiag;
typedef struct {
 uint32_t magic,sequence,elapsed_ms,raw,clear_read,ack_write,accepted_raw,suppressed_mask;
 uint32_t suppress_status[4],status_first[8],status_second[8];
 uint32_t held[2],gba,host,error,flags,poll_index,reserved;
} PeaqKeyRecord;
int peaq_key_capture_v3_dispatch(unsigned type,unsigned length);
#endif
