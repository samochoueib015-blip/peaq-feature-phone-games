#ifndef PEAQ_EMERALD_COLD_SAVE_H
#define PEAQ_EMERALD_COLD_SAVE_H
#include "../runtime/large_save.h"

#define PEAQ_COLD_SAVE_PAYLOAD 131072u
#define PEAQ_COLD_SAVE_RECORD (PEAQ_COLD_SAVE_PAYLOAD + 20u)
#define PEAQ_COLD_SAVE_SECTORS 257u

/* Fixed, preallocated ENGINE0.SAV / ENGINE1.SAV, each exactly 131092 bytes.
 * The caller must independently prove both complete FAT chains, exclusive
 * ownership (including against directories/other files), card identity and
 * lifetime. Sector numbers are copied at init and never taken from game input.
 * This component does not qualify a FAT mapping or a physical SD writer.
 * No allocation, FAT/directory changes, file resizing or device calls occur.
 */
typedef struct {
 void *cookie;
 /* Synchronous complete 512-byte operations, 1 success, anything else failure.
  * Buffers cannot be retained. sync waits for acknowledged card programming
  * completion, including after an uncertain write; it is not just a CPU fence.
  * The device adapter must enforce a real timeout and keep DMA memory owned
  * if stopping fails. All operations are serialized with other card users. */
 int (*read_sector)(void *,uint32_t,void *);
 int (*write_sector)(void *,uint32_t,const void *);
 int (*sync)(void *);
 uint32_t capacity_sectors,data_first,data_sectors;
} PeaqColdSaveHost;

typedef struct {
 uint32_t magic,busy,poisoned,writing,slot,position;
 uint32_t reads,writes,syncs;
 PeaqColdSaveHost host;
 uint32_t sectors[2][PEAQ_COLD_SAVE_SECTORS];
 uint8_t read_buffer[512],write_buffer[512];
} PeaqColdSave;

/* Caller owns separate aligned zero-initialized state, map and host.
 * Init does no I/O; duplicate/out-of-data-region sectors are rejected.
 * Reinitialization is refused, including after an I/O error. A poisoned
 * store requires a new session and card/filesystem validation.
 * Seed preallocated files with valid PLS1 records, not zero-length placeholders.
 */
int peaq_cold_save_init(PeaqColdSave *,const PeaqColdSaveHost *,
                        const uint32_t sectors[2][PEAQ_COLD_SAVE_SECTORS]);
/* Bind to the unchanged PeaqEmeraldServices.save / large_save interface.
 * out remains owned by the caller, separate from state. These callbacks are
 * for the trusted two-slot transaction engine, not an untrusted game's API.
 * They serialize each callback and reject reentry or a second open stream. */
int peaq_cold_save_bind(PeaqColdSave *,PeaqSaveHost *out);
/* Same result conventions as large_save. Payload is always exactly 128 KiB.
 * Only commit can write. It preserves the last valid slot and verifies the
 * complete new record after sync. Bytes beyond the EOF in the last sector are
 * preserved. Call only at an owned SD/cache boundary, never while DMA or the
 * game mutates the save payload. Context, payload, scratch and bytes are disjoint.
 */
int peaq_cold_save_load(PeaqColdSave *,void *,uint32_t *bytes,void *scratch);
int peaq_cold_save_commit(PeaqColdSave *,const void *,void *scratch);
#endif
