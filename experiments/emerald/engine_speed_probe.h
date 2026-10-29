#ifndef PEAQ_EMERALD_ENGINE_SPEED_PROBE_H
#define PEAQ_EMERALD_ENGINE_SPEED_PROBE_H
#include <stdint.h>
#include "sd_diag.h"
#define PEAQ_ENGINE_SPEED 0x7504u
#define PEAQ_ENGINE_SPEED_INFO 0x7505u
typedef struct {
 uint32_t magic,version,bytes,flags,stage,error,count,read_budget;
 uint32_t clock_before,clock_after,source_selector,bus_control;
 uint32_t baseline_bytes,baseline_crc0,baseline_crc1,baseline_ms;
 uint32_t fast_bytes,fast_crc0,fast_crc1,fast_ms;
 uint32_t reads_before,reads_after,commands_before,commands_after;
 uint32_t sd_error,poisoned,cleanup,required_crc,profile_crc32,total_ms,guards_ok,reserved;
} PeaqEngineSpeedReport;
int peaq_sd_fixed_speed(PeaqSdReport *);
int peaq_engine_speed_probe(PeaqSdReport *,PeaqEngineSpeedReport *);
#endif
