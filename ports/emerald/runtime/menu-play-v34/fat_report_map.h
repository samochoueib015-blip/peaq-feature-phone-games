#ifndef PEAQ_EMERALD_FAT_REPORT_MAP_H
#define PEAQ_EMERALD_FAT_REPORT_MAP_H
#include "fat_save_map.h"
#define PEAQ_REPORT_FILE_BYTES 512u
typedef struct {
 uint32_t magic,valid,error,cluster,sector;
 uint32_t hops,entries,directories,dir_clusters,seen_slots,seen_report;
 uint32_t mirror_base,mirror_cached,mirror_valid;
 uint8_t mirror[512];
} PeaqFatReportMap;
/* After a successful peaq_fat_save_map, independently audit the fixed
 * /GAMES/SMARAGD/REPORT.BIN. It must be writable, exactly 512 bytes and own
 * one exact EOC cluster, with no reachable file/directory alias. Only its
 * first sector is published, even when a cluster contains more sectors.
 *
 * The save result's header, 514 sectors, mirror cache and all PeaqFat file
 * handles/chains are preserved. Its queue[] and seen_dirs[] are scratch and
 * reused for this second bounded walk; no additional 4096-entry file slot.
 * Report must be separate, aligned and zero-initialized. A second call is
 * refused. Invalid report ownership does not invalidate a prior save map;
 * underlying read/format/budget failures can still poison PeaqFat normally.
 *
 * Returns 1 only after the complete audit. Failure publishes sector=0 and
 * valid=0. Caller must retain exclusive card ownership until its final write
 * and independently enforce the copied one-sector permit/card identity.
 * There is no filesystem or sector write operation in this component.
 */
int peaq_fat_report_map(PeaqFat *,PeaqFatSaveMap *,PeaqFatReportMap *);
#endif
