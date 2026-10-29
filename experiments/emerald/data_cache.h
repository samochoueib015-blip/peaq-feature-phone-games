#ifndef PEAQ_DATA_CACHE_H
#define PEAQ_DATA_CACHE_H
#include <stdint.h>

/* Separate cold-only experiment. Never callable under the stock OS.
 * The caller owns exception vectors and must run leave on EVERY exit.
 * SMP is established by the existing two-receipt protocol and retained.
 * No clocks, SD, flash, security registers or DMA descriptors are touched. */
#define PEAQ_DC_TABLE_BASE 0x81200000u
#define PEAQ_DC_TABLE_BYTES 0x8000u
#define PEAQ_DC_SCRATCH_BASE 0x81100000u
#define PEAQ_DC_SCRATCH_BYTES 0x40000u
#define PEAQ_DC_GAME_BEGIN 0x80410000u
#define PEAQ_DC_GAME_END 0x810d1000u
#define PEAQ_DC_LEVELS 2u

enum {DC_OK,DC_CONTEXT,DC_GEOMETRY,DC_TABLES,DC_READBACK,DC_ORDER,DC_RESTORE};
typedef struct {
 uint32_t ccsidr,line_shift,sets,ways,way_shift,bytes;
} PeaqDcLevel;
typedef struct {
 uint32_t error,snapshot,prepared,active,leaving,restored;
 uint32_t sctlr,actlr,ttbcr,ttbr0,ttbr1,dacr,csselr;
 uint32_t midr,ctr,clidr,cpsr;
 uint32_t tables_used,table_checksum,invalidate_ops,clean_ops;
 uint32_t entries,exits,observed_enabled;
 PeaqDcLevel level[PEAQ_DC_LEVELS];
} PeaqDcState;

extern volatile PeaqDcState peaq_dc;
extern uint32_t peaq_dc_tables[PEAQ_DC_TABLE_BYTES/4];
extern uint8_t __cache_text_end[];
/* Four-kilobyte/section identity map, all other addresses fault. */
int peaq_dc_build_tables(uint32_t text_end);
int peaq_dc_check_tables(uint32_t text_end);
int peaq_dc_decode_level(uint32_t ccsidr,PeaqDcLevel *out);
int peaq_dc_prepare(void);
int peaq_dc_enter(unsigned use_data);
int peaq_dc_leave(void);

uint32_t peaq_dc_r_sctlr(void),peaq_dc_r_actlr(void),peaq_dc_r_cpsr(void);
uint32_t peaq_dc_r_midr(void),peaq_dc_r_ctr(void),peaq_dc_r_clidr(void);
uint32_t peaq_dc_r_ttbcr(void),peaq_dc_r_ttbr0(void),peaq_dc_r_ttbr1(void);
uint32_t peaq_dc_r_dacr(void),peaq_dc_r_csselr(void),peaq_dc_r_ccsidr(void);
void peaq_dc_w_sctlr(uint32_t),peaq_dc_w_ttbcr(uint32_t);
void peaq_dc_w_ttbr0(uint32_t),peaq_dc_w_ttbr1(uint32_t),peaq_dc_w_dacr(uint32_t);
void peaq_dc_w_csselr(uint32_t);
void peaq_dc_invalidate_setway(uint32_t),peaq_dc_clean_setway(uint32_t);
void peaq_dc_barrier(void),peaq_dc_tlb(void),peaq_dc_instruction(void);
#endif
