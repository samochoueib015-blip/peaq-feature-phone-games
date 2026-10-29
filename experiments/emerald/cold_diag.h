#ifndef PEAQ_EMERALD_COLD_DIAG_H
#define PEAQ_EMERALD_COLD_DIAG_H
#include <stdint.h>

/* Separate cold FDL1 handoff only. Never an OS/PGA entry point. */
#define PEAQ_COLD_IMAGE 0x80100000u
#define PEAQ_COLD_STACK_LOW 0x800f0000u
#define PEAQ_COLD_ARENA 0x80400000u
#define PEAQ_COLD_ARENA_BYTES 0x01000000u
#define PEAQ_COLD_INFO 0x7100u
#define PEAQ_COLD_PROBE 0x7101u
#define PEAQ_COLD_REPLY 0x93u

/* All fields are little endian on the wire. No raw arbitrary-address API. */
typedef struct {
    uint32_t magic, version, bytes, flags;
    uint32_t chip_id, entered_sctlr, entered_cpsr, timer_ms;
    uint32_t image_begin, image_end, bss_end, stack_low, stack_high;
    uint32_t arena_begin, arena_bytes, probed_words, mismatch_words;
    uint32_t first_bad_address, probe_elapsed_ms, probe_count;
    uint32_t pinmap_crc32, keymap_crc32;
    uint32_t pin_count, key_count;
} PeaqColdReport;

/* Bits 0/1: cold cache/MMU precondition, validated embedded profile.
 * Bits 2/3: sparse probe completed/passed. It is NOT a full RAM test. */
void peaq_cold_probe(PeaqColdReport *report);
int peaq_cold_dispatch(unsigned type, unsigned length);
#endif
