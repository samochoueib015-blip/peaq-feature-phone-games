#ifndef PEAQ_EMERALD_SD_DIAG_H
#define PEAQ_EMERALD_SD_DIAG_H
#include <stdint.h>
#define PEAQ_SD_INFO 0x7300u
#define PEAQ_SD_INIT 0x7301u
#define PEAQ_SD_SECTOR0 0x7302u
#define PEAQ_SD_FILE 0x7303u
#define PEAQ_SD_MAGIC 0x31445350u /* PSD1 */
#define PEAQ_SD_FILE_BYTES 32768u
enum {
 SD_OK, SD_COLD_REQUIRED, SD_CHIP_MISMATCH, SD_ALREADY_RUN, SD_NOT_READY,
 SD_TIMER_ERROR, SD_ADI_TIMEOUT, SD_VOLTAGE_UNSUPPORTED, SD_RESET_TIMEOUT,
 SD_CLOCK_TIMEOUT, SD_INHIBIT_TIMEOUT, SD_COMMAND_TIMEOUT, SD_COMMAND_ERROR,
 SD_CARD_STATUS_ERROR, SD_V2_REQUIRED, SD_OCR_ERROR, SD_CSD_UNSUPPORTED,
 SD_SECTOR_RANGE, SD_DMA_GUARD, SD_READ_BUDGET, SD_MBR_INVALID,
 SD_BPB_INVALID, SD_CHAIN_INVALID, SD_DIRECTORY_LIMIT, SD_FILE_MISSING,
 SD_FILE_INVALID, SD_DATA_MISMATCH, SD_POWER_VERIFY
};
enum { SD_IDLE, SD_POWER, SD_PADS, SD_CLOCK, SD_IDENTIFY, SD_READY,
       SD_MBR, SD_VOLUME, SD_DIRECTORY, SD_FILE_DATA, SD_COMPLETE };
enum { SD_F_COLD=1, SD_F_BOARD=2, SD_F_POWER=4, SD_F_CARD=8,
       SD_F_SECTOR0=16, SD_F_FAT=32, SD_F_FILE=64, SD_F_POISONED=128 };
/* Fixed 240-byte little-endian report; even worst-case escaping fits512B. */
typedef struct {
 uint32_t magic,version,bytes,flags;
 uint32_t chip_id,entered_sctlr,entered_cpsr,timer_ms;
 uint32_t stage,error,init_count,sector_count;
 uint32_t file_count,total_polls,commands,reads;
 uint32_t last_cmd,last_arg,last_status,last_response;
 uint32_t controller_state,clock_control,cleanup,poisoned;
 uint32_t core_selector,io_selector,core_uv,io_uv;
 uint32_t host_ocr,card_ocr,rca,block_addressed;
 uint32_t cid[4],csd[4],capacity_lo,capacity_hi;
 uint32_t mbr_crc0,mbr_crc1,partition_lba,partition_sectors;
 uint32_t boot_crc,bpb_total,bpb_cluster,root_cluster;
 uint32_t fat_lba,data_lba,cluster_count,file_cluster;
 uint32_t file_bytes,file_crc0,file_crc1,file_mismatch;
 uint32_t elapsed_ms,profile_crc32;
} PeaqSdReport;
int peaq_sd_init(PeaqSdReport *r);
int peaq_sd_read(PeaqSdReport *r,uint32_t lba,uint8_t output[512]);
int peaq_sd_sector0(PeaqSdReport *r);
int peaq_sd_file(PeaqSdReport *r);
int peaq_sd_dispatch(unsigned type,unsigned length);
uint32_t peaq_sd_crc(uint32_t crc,const uint8_t *data,unsigned size);
#endif
