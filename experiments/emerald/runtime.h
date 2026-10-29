#ifndef PEAQ_EMERALD_RUNTIME_H
#define PEAQ_EMERALD_RUNTIME_H
#include <stdint.h>
#include "../runtime/module.h"
#include "../runtime/large_save.h"

/* A single Emerald instance in caller-owned RAM. No physical addresses, OS
 * calls, flash writes, or memory-size probes are performed here. A dedicated
 * phone mode must establish ownership before supplying its arena.
 * Services/config/state must lie outside the arena and remain alive until
 * successful stop or discard. The runtime state must not alias either config.
 * All calls are serialized on one execution context; never invoke from IRQs.
 */
typedef struct {
 void *cookie;
 int (*module_read)(void *,void *,uint32_t,uint32_t *);
 void (*sync_code)(void *,uint32_t);
 void (*present)(void *,const uint16_t *,uint32_t,uint32_t);
 /* Consume/copy samples before returning; 1 means accepted. May be NULL. */
 int (*audio)(void *,const int8_t *,uint32_t,uint32_t);
 const PeaqSaveHost *save;
} PeaqEmeraldServices;

enum {
 PEAQ_EMERALD_OK=1, PEAQ_EMERALD_IDLE=0,
 PEAQ_EMERALD_ARGUMENT=-1, PEAQ_EMERALD_BUSY=-2,
 PEAQ_EMERALD_MODULE=-3, PEAQ_EMERALD_MEMORY=-4,
 PEAQ_EMERALD_SAVE_IO=-5, PEAQ_EMERALD_SAVE_INVALID=-6,
 PEAQ_EMERALD_INIT=-7
};
typedef struct {
 const PeaqEmeraldServices *services;
 uint8_t *arena;
 uint32_t arena_bytes,used;
 PeaqModule module;
 uint16_t *gba,*portrait;
 uint8_t *layers,*save_scratch,*save_memory;
 uint32_t active,paused,dirty,generation,quiet_frames;
 uint32_t previous_ms,phase,frames,audio_dropped;
 int32_t last_error;
} PeaqEmeraldRuntime;

/* Returns OK only after bounded module validation, save restoration and init.
 * Both absent slots start with a blank cartridge; present invalid saves refuse
 * startup. The supplied save host must distinguish absence from I/O failure.
 */
int peaq_emerald_runtime_start(PeaqEmeraldRuntime *,const PeaqEmeraldServices *,
                              void *arena,uint32_t bytes,uint32_t now_ms);
/* At most one frame per call, paced to a nominal 60Hz with no catch-up burst.
 * buttons are GBA bits A/B/Select/Start/Right/Left/Up/Down/R/L (0..9).
 * This is a scheduling target, not a phone-performance claim.
 */
int peaq_emerald_runtime_step(PeaqEmeraldRuntime *,uint32_t now_ms,uint32_t buttons);
void peaq_emerald_runtime_pause(PeaqEmeraldRuntime *,int paused,uint32_t now_ms);
int peaq_emerald_runtime_flush(PeaqEmeraldRuntime *);
/* Save failure keeps the instance alive and paused, so the caller can retry
 * or explicitly offer discard. Never unload an engine inside a host callback.
 */
int peaq_emerald_runtime_stop(PeaqEmeraldRuntime *);
void peaq_emerald_runtime_discard(PeaqEmeraldRuntime *);
#endif
