#ifndef PEAQ_DOOM_READ_CACHE_H
#define PEAQ_DOOM_READ_CACHE_H
#include <stdint.h>
#include "fileio.h"

/* Worker-owned, two bounded 4-KiB read-ahead blocks shared by eight handles.
 * Used only with immutable, read-only files for this engine generation. The
 * remote callback copies through the UI channel; it is NEVER a native SFS
 * pointer. No allocation, OS call, synchronization or UI access occurs here.
 * Logical positions differ from native positions after read-ahead/cache seek.
 * Failed read/seek discards cached bytes and poisons that handle until a real
 * successful seek. Ambiguous close is never retried. */
#define PEAQ_DOOM_CACHE_BLOCKS 2u
#define PEAQ_DOOM_CACHE_BYTES 4096u
typedef int32_t (*PeaqDoomRemoteIo)(void *,uint32_t,uint32_t,uint32_t,uint32_t);
typedef struct {
 uint32_t fd,position,remote_position,poisoned,close_uncertain;
} PeaqDoomCachedFile;
typedef struct {
 uint32_t fd,start,length,short_read;
 uint8_t data[PEAQ_DOOM_CACHE_BYTES];
} PeaqDoomCachedBlock;
typedef struct {
 void *cookie;PeaqDoomRemoteIo remote;
 PeaqDoomCachedFile files[PEAQ_DOOM_FILE_SLOTS];
 PeaqDoomCachedBlock blocks[PEAQ_DOOM_CACHE_BLOCKS];
 uint32_t victim;
} PeaqDoomReadCache;
void peaq_doom_read_cache_init(PeaqDoomReadCache *,PeaqDoomRemoteIo,void *);
int32_t peaq_doom_read_cache_io(PeaqDoomReadCache *,uint32_t,uint32_t,uint32_t,uint32_t);
#endif
