#ifndef PEAQ_SD_SAVE_BOARD_H
#define PEAQ_SD_SAVE_BOARD_H
#include "sd_diag.h"
#define PEAQ_SD_SAVE_MAP_SIZE 2u
typedef struct {
 uint32_t cid[4],data_first,data_sectors,write_limit;
 uint32_t sectors[PEAQ_SD_SAVE_MAP_SIZE];
} PeaqSdSavePermit;
typedef struct {
 uint32_t armed,writes,status_queries,ready_checks,error;
} PeaqSdSaveStatus;
/* Separate experimental writer. Caller must first establish the map's file
 * ownership; this is not a FAT validator. Fixed copied map, once per session,
 * no erase/format/multi-block writes. Initial qualification permits at most
 * 514 single-sector write attempts. The read-only driver remains unchanged.
 */
int peaq_sd_save_arm(PeaqSdReport *,const PeaqSdSavePermit *);
int peaq_sd_save_write(PeaqSdReport *,uint32_t lba,const uint8_t data[512]);
int peaq_sd_save_sync(PeaqSdReport *);
const PeaqSdSaveStatus *peaq_sd_save_status(void);
#endif
