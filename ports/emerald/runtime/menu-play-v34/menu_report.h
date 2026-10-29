#ifndef PEAQ_MENU_REPORT_H
#define PEAQ_MENU_REPORT_H
#include "fat_report_map.h"
/* Only touched at the uncached outer boundary. This is a separate permanent
 * reservation, not another FAT handle or a pointer into game-owned memory. */
typedef struct {
 uint32_t before[16];
 PeaqFatReportMap map;
 uint32_t words[128],verify[128];
 uint32_t sequence,error,started_ms,save_active,save_reads,save_writes,save_phase,save_percent;
 uint32_t after[16];
} PeaqMenuReportState;
_Static_assert(sizeof(PeaqMenuReportState)<=4096,"report reservation exceeded");
#endif
