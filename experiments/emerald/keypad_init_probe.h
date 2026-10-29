#ifndef PEAQ_KEYPAD_INIT_PROBE_H
#define PEAQ_KEYPAD_INIT_PROBE_H
#include <stdint.h>
#define KIP_INFO 0x7a00u
#define KIP_RUN 0x7a01u
#define KIP_REPLY 0x9du
#define KIP_MAGIC 0x3250494bu /* KIP2 */
enum {KIP_F_COLD=1,KIP_F_BOARD=2,KIP_F_PADS=4,KIP_F_CLOCK=8,KIP_F_BEFORE=16,KIP_F_AFTER=32,KIP_F_DONE=64,KIP_F_ERROR=128};
enum {KIP_IDLE,KIP_PADS,KIP_CLOCK,KIP_BEFORE_RESET,KIP_RESET_ASSERTED,KIP_RESET_RELEASED,KIP_AFTER_RESET,KIP_DONE,KIP_FAILED};
enum {KIP_OK,KIP_CHIP_ERROR,KIP_CPU_ERROR,KIP_GUARD_ERROR};
/*40 little-endian words. No snapshot or KPD read is performed before clock.
 * before/after CTRL values are observations, not expected/allowed masks.
 * Config_count and kpd_write_count must remain zero throughout this image.
 */
typedef struct {
 uint32_t magic,version,bytes,flags,chip_id,sctlr,cpsr,current_cpsr;
 uint32_t stage,error,run_count,timer_ms,config_count,kpd_write_count,clock_write_count,reset_write_count;
 uint32_t before_clock_valid,between_valid,after_valid,snapshot_count;
 uint32_t between_timer,between_control,between_raw,between_clear,between_status;
 uint32_t after_timer,after_control,after_raw,after_clear,after_status;
 uint32_t pin_crc,pin_count,image_end,bss_end,guard_ok,kpd_read_count,reset_iterations,config_skipped,reserved0,reserved1;
} PeaqKeyInitProbe;
int peaq_key_init_probe_dispatch(unsigned type,unsigned length);
#endif
