#ifndef PEAQ_EMERALD_ICACHE_INTRO_H
#define PEAQ_EMERALD_ICACHE_INTRO_H
#include <stdint.h>
#define PEAQ_CACHE_INFO 0x7900u
#define PEAQ_CACHE_INIT 0x7901u
#define PEAQ_CACHE_SPEED 0x7902u
#define PEAQ_CACHE_START 0x7903u
#define PEAQ_CACHE_SMP 0x7904u
#define PEAQ_CACHE_RECEIPT1 0x7905u
#define PEAQ_CACHE_RECEIPT2 0x7906u
#define PEAQ_CACHE_RELEASE 0x7907u
#define PEAQ_CACHE_FIRST 0x7908u
#define PEAQ_CACHE_BATCH 0x7909u
#define PEAQ_CACHE_INTRO_INFO 0x790au
#define PEAQ_CACHE_DISPLAY_INFO 0x790bu
#define PEAQ_CACHE_SPEED_INFO 0x790cu
#define PEAQ_CACHE_SD_INFO 0x790du
#define PEAQ_CACHE_FAULT0 0x790eu
#define PEAQ_CACHE_FAULT1 0x790fu
#define PEAQ_CACHE_REPLY 0x9cu
#define PEAQ_CACHE_MAGIC 0x31524349u /* ICR1:48 little-endian words,192B */
enum {CACHE_IDLE,CACHE_STARTED,CACHE_SMP_ON,CACHE_RECEIPT_ONE,CACHE_RECEIPT_TWO,
 CACHE_RELEASED,CACHE_RUNNING,CACHE_COMPLETE,CACHE_FAILED};
enum {CACHE_OK,CACHE_ORDER,CACHE_REPEAT,CACHE_CONTEXT,CACHE_READBACK,CACHE_GUARD,
 CACHE_CPU_FAULT,CACHE_ENGINE,CACHE_USB_STATE,CACHE_STATE};
enum {CACHE_F_COLD=1,CACHE_F_STARTED=2,CACHE_F_SMP=4,CACHE_F_RECEIPT1=8,
 CACHE_F_RECEIPT2=16,CACHE_F_RELEASED=32,CACHE_F_COMPLETE=64,CACHE_F_FAILED=128,
 CACHE_F_POWER_CYCLE=256,CACHE_F_ICACHE_VERIFIED=512,CACHE_F_USB_SAFE=1024,
 CACHE_F_CPU_FAULT=2048};
typedef struct {
 uint32_t magic,version,bytes,flags;
 uint32_t stage,error,last_command,reply_serial;
 uint32_t smp_attempts,gate_receipts,release_count,batch_count;
 uint32_t frames,frames_target,batch_limit,last_batch_frames;
 uint32_t entered_sctlr,entered_actlr,current_sctlr,current_actlr;
 uint32_t midr,cpsr_before,cpsr_after,smp_readback;
 uint32_t armed_sctlr,enabled_sctlr,disabled_sctlr,outer_restored_sctlr;
 uint32_t enable_attempts,disable_attempts,icache_invalidations,bp_invalidations;
 uint32_t target_entered,target_returned,cpu_faults,exception_kind;
 uint32_t fault_pc,dfsr,dfar,control_restored;
 uint32_t guards_ok,terminal,awaiting_power_cycle,usb_gate_ok;
 uint32_t last_batch_ms,total_batch_ms,transition_ms,reserved;
} PeaqCacheReport;
int peaq_cache_dispatch(unsigned,unsigned);
uint32_t peaq_cache_read_sctlr(void);
uint32_t peaq_cache_read_actlr(void);
uint32_t peaq_cache_read_cpsr(void);
uint32_t peaq_cache_read_midr(void);
void peaq_cache_write_sctlr(uint32_t);
void peaq_cache_write_actlr(uint32_t);
void peaq_cache_maintenance(void);
void peaq_cache_fault_leave(void);
__attribute__((noreturn)) void peaq_cache_halt(void);
#endif
