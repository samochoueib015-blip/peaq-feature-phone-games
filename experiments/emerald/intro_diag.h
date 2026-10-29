#ifndef PEAQ_EMERALD_INTRO_DIAG_H
#define PEAQ_EMERALD_INTRO_DIAG_H
#include <stdint.h>
#define PEAQ_INTRO_INFO 0x7600u
#define PEAQ_INTRO_INIT 0x7601u
#define PEAQ_INTRO_SPEED 0x7602u
#define PEAQ_INTRO_START 0x7603u
#define PEAQ_INTRO_BATCH 0x7604u
#define PEAQ_INTRO_DISPLAY_INFO 0x7605u
#define PEAQ_INTRO_SPEED_INFO 0x7606u
#define PEAQ_INTRO_SD_INFO 0x7607u
#define PEAQ_INTRO_MAGIC 0x31494c45u /* ELI1 */
#define PEAQ_INTRO_ARENA_BYTES 13363488u
#define PEAQ_INTRO_FRAMES 240u
#define PEAQ_INTRO_BATCH_FRAMES 16u
enum {INTRO_OK,INTRO_ORDER,INTRO_REPEAT,INTRO_GUARD,INTRO_DISPLAY_INIT,INTRO_SD,
 INTRO_CAPACITY,INTRO_FAT,INTRO_GEOMETRY,INTRO_FILE,INTRO_SIZE,INTRO_HEADER,
 INTRO_CRC,INTRO_RUNTIME,INTRO_PRESENT,INTRO_SAVE,INTRO_TIMER};
enum {INTRO_IDLE,INTRO_HW_READY,INTRO_SPEED_READY,INTRO_STARTED,INTRO_RUNNING,INTRO_COMPLETE,INTRO_FAILED};
enum {INTRO_F_COLD=1,INTRO_F_BOARD=2,INTRO_F_DISPLAY=4,INTRO_F_CARD=8,INTRO_F_SPEED=16,
 INTRO_F_STARTED=32,INTRO_F_COMPLETE=64,INTRO_F_FAILED=128};
typedef struct {
 uint32_t magic,version,bytes,flags,chip_id,entered_sctlr,entered_cpsr,timer_ms;
 uint32_t stage,error,init_count,start_count,batch_count,frames,frames_target,batch_target;
 uint32_t runtime_result,runtime_error,display_result,display_error,display_poisoned,present_attempts,present_count,callback_fault;
 uint32_t load_ms,step_ms,last_batch_ms,wall_ms,module_reads,stream_bytes,file_crc,header_matched;
 uint32_t arena_start,arena_bytes,arena_used,guards_ok,module_base,image_bytes,memory_bytes,exports;
 uint32_t gba,portrait,layers,save_memory,sd_error,fs_error,sd_reads,fs_reads;
 uint32_t write_attempts,save_dirty,save_generation,save_read_flags,clock_control,sd_poisoned,required_divider,source_profile;
 uint32_t file_bytes,speed_passed,reserved,reserved2;
} PeaqIntroReport;
int peaq_intro_dispatch(unsigned,unsigned);
#endif
