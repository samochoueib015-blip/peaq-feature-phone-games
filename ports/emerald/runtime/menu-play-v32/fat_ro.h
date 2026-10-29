#ifndef PEAQ_EMERALD_FAT_RO_H
#define PEAQ_EMERALD_FAT_RO_H
#include <stdint.h>
#define PEAQ_FAT_FILES 3u
#define PEAQ_FAT_CHAIN_MAX 4096u
#define PEAQ_FAT_READ_MAX 4096u
#define PEAQ_FAT_SECTOR_BUDGET_MAX 131072u
enum { PEAQ_FAT_ENGINE,PEAQ_FAT_SAVE0,PEAQ_FAT_SAVE1 };
enum { PEAQ_FAT_OK=1,PEAQ_FAT_ABSENT=0,PEAQ_FAT_ARGUMENT=-1,
 PEAQ_FAT_IO=-2,PEAQ_FAT_FORMAT=-3,PEAQ_FAT_LIMIT=-4,
 PEAQ_FAT_HANDLE=-5,PEAQ_FAT_BUSY=-6 };
typedef struct {
 void *cookie;
 /* Exactly512 complete bytes, 1 success / anything else failure.
  * Serial synchronous callback, no writes, no ownership transfer. */
 int (*read_sector)(void *,uint32_t,uint8_t *);
 uint32_t capacity_sectors,read_budget;
} PeaqFatHost;
typedef struct {
 uint32_t token,role,bytes,position,clusters,live;
 uint32_t chain[PEAQ_FAT_CHAIN_MAX];
} PeaqFatFile;
/* Caller owns this zero-initialized context for the entire cold session.
 * Do not reset/remount it while handles or callbacks exist. No heap/MMIO here.
 * Three fixed roles; chain maps make seek bounded and independent of later FAT
 * contents. Context, host and output buffers must not overlap.
 */
typedef struct {
 uint32_t magic,mounted,next_token,reads,budget,capacity;
 int32_t error;
 PeaqFatHost host;
 uint32_t partition,total,spc,fat_lba,fat_sectors,data_lba,cluster_count,root;
 uint32_t fat_cached,data_cached,fat_valid,data_valid;
 uint32_t folder,folder_known,folder_absent;
 uint8_t fat[512],data[512];
 PeaqFatFile files[PEAQ_FAT_FILES];
} PeaqFat;
typedef struct {PeaqFat *fs;uint32_t handle;} PeaqFatStream;
/* Zero-initialize, then set fs. This owns the two save-role handles until the
 * cold session ends. The engine handle remains independently usable. */
typedef struct {PeaqFat *fs;uint32_t handle[2],bytes[2],known,absent;} PeaqFatSaves;
/* mount=1 success; open=1 success,0 positively absent,negative failure.
 * Each role is a fixed path below /GAMES/SMARAGD: ENGINE.PGE or ENGINE[01].SAV.
 * No arbitrary names, LFN, writes or recursive enumeration. */
int peaq_fat_mount(PeaqFat *,const PeaqFatHost *);
int peaq_fat_open(PeaqFat *,uint32_t role,uint32_t *handle,uint32_t *bytes);
int peaq_fat_close(PeaqFat *,uint32_t handle);
int peaq_fat_seek(PeaqFat *,uint32_t handle,uint32_t absolute);
int peaq_fat_read(PeaqFat *,uint32_t handle,void *,uint32_t,uint32_t *);
/* Exact PeaqModuleHost.read signature: success including EOF =>1/got0;
 * failures=>0. Valid, nonaliasing count outputs are zeroed on read failure.
 * Set stream from a successful ENGINE open at position0. */
int peaq_fat_module_read(void *,void *,uint32_t,uint32_t *);
/* PeaqSaveHost.read only: 1 success/EOF, 0 definitely absent, -1 failure.
 * Open/format/read errors never become absence. ENGINE0.SAV / ENGINE1.SAV
 * are distinct from the OS adapter's long filenames engine-save[01].dat.
 * No begin/write/finish implementation: this does NOT make saves durable. */
int peaq_fat_save_read(void *,uint32_t,uint32_t,void *,uint32_t,uint32_t *);
#endif
