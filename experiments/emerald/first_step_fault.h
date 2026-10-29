#ifndef PEAQ_FIRST_STEP_FAULT_H
#define PEAQ_FIRST_STEP_FAULT_H
#include <stdint.h>
/* FST1: 96 little-endian words, fixed 384 bytes. */
#define PEAQ_FAULT_MAGIC 0x31545346u
#define PEAQ_FAULT_GUARD 0x71a694d3u
enum {FAULT_ARMED=1,FAULT_RETURNED=2,FAULT_CAPTURED=4,FAULT_RESTORED=8,FAULT_GUARDS=16};
typedef struct {
 uint32_t magic,version,bytes,flags;
 uint32_t stage,error,attempts,exception_kind;
 uint32_t breadcrumb,step_result,vector_base,armed_sctlr;
 uint32_t original_sctlr,original_vbar,restored_sctlr,restored_vbar;
 uint32_t cpsr_before,fault_cpsr,fault_spsr,fault_lr;
 uint32_t fault_pc,fault_pc_valid,exception_sp,interrupted_sp;
 uint32_t dfsr,dfar,ifsr,ifar;
 uint32_t adfsr,aifsr,contextidr,register_mask;
 uint32_t r[13],exception_stack_index,call_sp,return_lr;
 uint32_t midr,mpidr,revidr,ctr;
 uint32_t pfr0,pfr1,dfr0,afr0;
 uint32_t mmfr0,mmfr1,mmfr2,mmfr3;
 uint32_t isar0,isar1,isar2,isar3;
 uint32_t isar4,isar5,clidr,actlr;
 uint32_t cpacr,ttbcr,ttbr0,ttbr1;
 uint32_t dacr,vbar,sctlr,cpsr_after;
 uint32_t stack_guards_ok,active,finish_count,control_restored;
 uint32_t old_sp_abt,old_sp_und,old_sp_svc,old_sp_irq;
 uint32_t old_sp_fiq,old_lr_abt,old_lr_und,old_lr_svc;
 uint32_t old_lr_irq,old_lr_fiq,fn,fn_arg;
 uint32_t step_ms,initial_fault_status_valid,hcr_observed,reserved;
} PeaqFirstStepFault;
extern volatile PeaqFirstStepFault peaq_fault;
/* Own 5 * (64 guard + 512 stack + 64 guard) bytes. */
extern uint32_t peaq_fault_stacks[5][160];
extern const uint32_t peaq_fault_vectors[];
int peaq_fault_call(int (*fn)(void *),void *arg);
int peaq_first_dispatch(unsigned type,unsigned length);
#endif
