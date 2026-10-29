#ifndef PEAQ_DATA_CACHE_DIAG_H
#define PEAQ_DATA_CACHE_DIAG_H
#include <stdint.h>
#define PEAQ_DCP_MAGIC 0x31504344u
#define PEAQ_DCP_INFO 0x7d00u
#define PEAQ_DCP_SMP 0x7d01u
#define PEAQ_DCP_RECEIPT1 0x7d02u
#define PEAQ_DCP_RECEIPT2 0x7d03u
#define PEAQ_DCP_PREPARE 0x7d04u
#define PEAQ_DCP_BASELINE 0x7d05u
#define PEAQ_DCP_CACHED 0x7d06u
#define PEAQ_DCP_CPU 0x7d07u
#define PEAQ_DCP_FAULT0 0x7d08u
#define PEAQ_DCP_FAULT1 0x7d09u
typedef struct {
 uint32_t magic,version,bytes,stage,error,last_command,replies,smp;
 uint32_t entry_sctlr,entry_actlr,entry_cpsr,chip_id;
 uint32_t current_sctlr,current_actlr,current_cpsr,guards;
 uint32_t baseline_count,cached_count,baseline_ms,cached_ms;
 uint32_t baseline_outer_ms,cached_outer_ms,baseline_sum,cached_sum;
 uint32_t baseline_ram_sum,cached_ram_sum,expected_sum,words;
 uint32_t passes,image_end,bss_end,terminal;
} PeaqCacheDiagnostic;
int peaq_dc_dispatch(unsigned,unsigned);
#endif
