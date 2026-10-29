#ifndef PEAQ_EMERALD_INTERACTIVE_DIAG_H
#define PEAQ_EMERALD_INTERACTIVE_DIAG_H
#include <stdint.h>

/* Separate fixed-cache RAM diagnostic. All request payloads are empty.
 * The 250-ms gate proves a logically quiet fresh decoder, not that a key
 * already held before INIT cannot escape hardware edge reporting. The host
 * must instruct the owner to release all keys during Load/Init and before GO.
 * No writer, audio device, OS return, arbitrary address or input-mask command.
 */
#define PEAQ_INTERACTIVE_MAGIC 0x31524945u /* EIR1 */
#define PEAQ_INTERACTIVE_REPLY 0xa1u
#define PEAQ_INTERACTIVE_INFO 0x7c00u
#define PEAQ_INTERACTIVE_INIT 0x7c01u
#define PEAQ_INTERACTIVE_SPEED 0x7c02u
#define PEAQ_INTERACTIVE_LOAD 0x7c03u
#define PEAQ_INTERACTIVE_KEYS_INIT 0x7c04u
#define PEAQ_INTERACTIVE_QUIET 0x7c05u
#define PEAQ_INTERACTIVE_SMP 0x7c06u
#define PEAQ_INTERACTIVE_RECEIPT1 0x7c07u
#define PEAQ_INTERACTIVE_RECEIPT2 0x7c08u
#define PEAQ_INTERACTIVE_RELEASE 0x7c09u
#define PEAQ_INTERACTIVE_GO 0x7c0au
#define PEAQ_INTERACTIVE_STEP 0x7c0bu
#define PEAQ_INTERACTIVE_STOP 0x7c0cu
#define PEAQ_INTERACTIVE_KEYS_INFO 0x7c0du /* KIN1,104B,type9f */
#define PEAQ_INTERACTIVE_FAULT0 0x7c0eu /* FST1 first192B,type97 */
#define PEAQ_INTERACTIVE_FAULT1 0x7c0fu /* FST1 last192B,type98 */
#define PEAQ_INTERACTIVE_INTRO_INFO 0x7c10u /* ELI1,240B,type95 */
#define PEAQ_INTERACTIVE_DISPLAY_INFO 0x7c11u /* FRM1,192B,type93 */
#define PEAQ_INTERACTIVE_SD_INFO 0x7c12u /* SD,240B,type96 */
#define PEAQ_INTERACTIVE_SPEED_INFO 0x7c13u /* Speed,128B,type94 */
#define PEAQ_INTERACTIVE_CACHE_INFO 0x7c14u /* ICR1,192B,type9c */
#define PEAQ_INTERACTIVE_RECORD0 0x7c20u /* 32 terminal-only144B KPR3 slots,typea0 */
#define PEAQ_INTERACTIVE_RECORDS 32u
#define PEAQ_INTERACTIVE_FRAME_LIMIT 72000u
#define PEAQ_INTERACTIVE_REQUEST_LIMIT 60000u
#define PEAQ_INTERACTIVE_SESSION_MS 1800000u
#define PEAQ_INTERACTIVE_HOST_IDLE_MS 5000u
#define PEAQ_INTERACTIVE_QUIET_MS 250u
#define PEAQ_INTERACTIVE_SAVE_VOLATILE_ONLY 1u

enum {
 INTERACTIVE_IDLE,INTERACTIVE_HW_READY,INTERACTIVE_SPEED_READY,
 INTERACTIVE_LOADED,INTERACTIVE_KEYS_READY,INTERACTIVE_QUIET_READY,
 INTERACTIVE_SMP_ON,INTERACTIVE_RECEIPT_ONE,INTERACTIVE_RECEIPT_TWO,
 INTERACTIVE_RELEASED,INTERACTIVE_RUNNING,INTERACTIVE_STOPPED,INTERACTIVE_FAILED
};
enum {
 INTERACTIVE_OK,INTERACTIVE_ORDER,INTERACTIVE_REPEAT,INTERACTIVE_KEY_INIT,
 INTERACTIVE_KEY_READ,INTERACTIVE_TIMER,INTERACTIVE_SAVE,INTERACTIVE_ENGINE,
 INTERACTIVE_CPU,INTERACTIVE_TRANSPORT,INTERACTIVE_GUARD,INTERACTIVE_REQUEST_BUDGET
};
enum {
 INTERACTIVE_STOP_NONE,INTERACTIVE_STOP_BACK,INTERACTIVE_STOP_HOST,
 INTERACTIVE_STOP_DEADLINE,INTERACTIVE_STOP_FRAME_LIMIT,INTERACTIVE_STOP_HOST_IDLE,
 INTERACTIVE_STOP_SAVE,INTERACTIVE_STOP_INPUT,INTERACTIVE_STOP_ENGINE,
 INTERACTIVE_STOP_CPU,INTERACTIVE_STOP_TRANSPORT,INTERACTIVE_STOP_TIMER,
 INTERACTIVE_STOP_REQUEST_LIMIT
};
enum {
 INTERACTIVE_F_COLD=1,INTERACTIVE_F_LOADED=2,INTERACTIVE_F_KEYS=4,
 INTERACTIVE_F_QUIET=8,INTERACTIVE_F_SMP=16,INTERACTIVE_F_RECEIPT1=32,
 INTERACTIVE_F_RECEIPT2=64,INTERACTIVE_F_RELEASED=128,
 INTERACTIVE_F_RUNNING=256,INTERACTIVE_F_STOPPED=512,INTERACTIVE_F_FAILED=1024,
 INTERACTIVE_F_VOLATILE=2048,INTERACTIVE_F_NO_AUDIO=4096
};
/* 60 little-endian u32 words =240B. quiet_ready is LOGICAL quiet only.
 * Before GO, QUIET may repeat; GO with a no-longer-quiet input returns status
 * without starting or claiming an error. Host can run QUIET and retry GO.
 * Once stopped, no second GO/LOAD/STEP; STOP itself is idempotent.
 * Final status fields freeze except last_command/reply_serial and live CPU
 * readback fields. Record slot=(sequence-1)%32, sequence is lifetime-global.
 */
typedef struct {
 uint32_t magic,version,bytes,flags;
 uint32_t stage,error,last_command,reply_serial;
 uint32_t frames,frame_limit,session_limit_ms,session_ms;
 uint32_t step_requests,request_limit,active,stopped;
 uint32_t stop_reason,stop_requested,logical_ms,host_idle_limit_ms;
 uint32_t start_ms,last_request_ms,step_last_ms,step_total_ms;
 uint32_t input_polls,input_records,input_ring_overwrites,input_max_gap_ms;
 uint32_t held0,held1,gba,host;
 uint32_t input_presses,input_releases,input_longs,input_error;
 uint32_t quiet_required_ms,quiet_ms,quiet_ready,owner_release_instruction_required;
 uint32_t save_policy,save_dirty,save_generation,write_attempts;
 uint32_t cache_smp,cache_enabled_count,cache_disabled_count,cache_faults;
 uint32_t current_sctlr,current_actlr,cpu_restored,guards_ok;
 uint32_t display_frames,display_error,sd_error,sd_write_commands;
 uint32_t runtime_error,terminal,awaiting_power_cycle,reserved;
} PeaqInteractiveReport;
int peaq_interactive_dispatch(unsigned type,unsigned length);
/* Called only from the new private transport's bounded wait sites.
 * Never runs engine code, sends USB, or invokes SD. */
void peaq_interactive_wait_hook(void);
void peaq_interactive_transport_failure(void);
#endif
