#ifndef PEAQ_EMERALD_ENGINE_LOAD_DIAG_H
#define PEAQ_EMERALD_ENGINE_LOAD_DIAG_H
#include <stdint.h>
#define PEAQ_ENGINE_INFO 0x7500u
#define PEAQ_ENGINE_INIT 0x7501u
#define PEAQ_ENGINE_OPEN 0x7502u
#define PEAQ_ENGINE_LOAD 0x7503u
#define PEAQ_ENGINE_MAGIC 0x31444c45u /* ELD1 */
enum { ELD_OK,ELD_ORDER,ELD_REPEAT,ELD_SD,ELD_CAPACITY,ELD_FAT,ELD_GEOMETRY,
 ELD_FILE,ELD_SIZE,ELD_HEADER,ELD_MEMORY,ELD_PARSE,ELD_CRC,ELD_GUARD,ELD_STREAM,ELD_SPEED };
enum { ELD_IDLE,ELD_CARD_READY,ELD_FILESYSTEM,ELD_FILE_OPEN,ELD_VERIFIED,ELD_FAILED };
enum { ELD_F_COLD=1,ELD_F_BOARD=2,ELD_F_CARD=4,ELD_F_FAT=8,ELD_F_FILE=16,ELD_F_VERIFIED=32,ELD_F_FAILED=64,ELD_F_SPEED=128 };
typedef struct {
 uint32_t magic,version,bytes,flags,chip_id,entered_sctlr,entered_cpsr,timer_ms;
 uint32_t stage,error,init_count,load_count,sd_error,fs_error,module_result,elapsed_ms;
 uint32_t capacity,partition_lba,partition_sectors,spc,fat_lba,data_lba,cluster_count,root_cluster;
 uint32_t file_bytes,file_crc,boot_crc,header_matched,image_bytes,memory_bytes,relocations,exports;
 uint32_t allocation,allocation_bytes,arena_limit,guards_ok,file_state,file_state_bytes,module_reads,stream_bytes;
 uint32_t sd_reads,fs_reads,sd_commands,sd_polls,last_cmd,last_arg,sd_status,sd_response;
 uint32_t clock_control,controller_state,poisoned,cleanup,core_uv,io_uv,profile_crc32,required_divider;
 uint32_t load_ms,parse_done,release_count,mount_count;
} PeaqEngineLoadReport;
int peaq_engine_load_dispatch(unsigned,unsigned);
#endif
