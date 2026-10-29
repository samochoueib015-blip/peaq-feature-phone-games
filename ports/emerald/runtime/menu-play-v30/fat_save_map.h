#ifndef PEAQ_EMERALD_FAT_SAVE_MAP_H
#define PEAQ_EMERALD_FAT_SAVE_MAP_H
#include "fat_ro.h"
#include "cold_save.h"
#define PEAQ_SAVE_MAP_DIRS 128u
#define PEAQ_SAVE_MAP_DIR_CLUSTERS 512u
#define PEAQ_SAVE_MAP_HOPS 131072u
typedef struct {
 uint32_t first,parent;
} PeaqSaveDirectory;
/* Temporary, caller-owned audit workspace. No I/O or write capability is
 * retained in the result. Only valid==1 publishes the two complete maps.
 * A full reachable-directory walk rejects any other owner of a save cluster.
 * Limits are deliberate: a bigger/ambiguous filesystem is refused, not partly
 * checked. Detached/orphan allocations and arbitrary card damage are outside
 * this contract. Use with exclusive card ownership until the session ends.
 */
typedef struct {
 uint32_t magic,valid,error,hops,entries,directories,dir_clusters,seen_slots;
 uint32_t mirror_base,mirror_cached,mirror_valid;
 uint32_t sectors[2][PEAQ_COLD_SAVE_SECTORS];
 PeaqSaveDirectory queue[PEAQ_SAVE_MAP_DIRS];
 uint32_t seen_dirs[PEAQ_SAVE_MAP_DIR_CLUSTERS];
 uint8_t mirror[512];
} PeaqFatSaveMap;
/* Opens both fixed save roles, requires exact preallocated PLS1 record sizes,
 * exact EOC chains, writable entries and unique ownership. Checks mirrored FAT
 * sectors encountered in the walk, including every save-chain entry. No FAT,
 * directory, allocation or file data writes. Result1 only on a complete audit.
 * Caller must additionally validate record contents and the card identity.
 */
int peaq_fat_save_map(PeaqFat *,PeaqFatSaveMap *);
#endif
