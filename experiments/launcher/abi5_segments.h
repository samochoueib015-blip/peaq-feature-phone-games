#ifndef PEAQ_EXPERIMENTAL_PGA5_SEGMENTS_H
#define PEAQ_EXPERIMENTAL_PGA5_SEGMENTS_H
#include <stdint.h>
#include "../../sdk/pga.h"
#include "../runtime/resident.h"

/* ABI5 descriptor REVISION2, experimental only. The installed SDK/loader and
 * the historical single-arena ABI5-v1 experiment remain unchanged.
 * Same 10-word PGA header / 3272-byte metadata; header[9] is the descriptor
 * offset. File length/hash/relocation/cache checks MUST precede preparation.
 * No descriptor or table word may be a relocation target. This is a trusted
 * native lifetime format, not an ARM sandbox.
 *
 * Revision2 uses flags2 and declares a bounded table of independent segments.
 * IDs are unique nonzero game-defined numbers, not a firmware game registry.
 * Each SegmentSpec has flags0 and alignment4/8/16/32/64. sizes describe usable
 * bytes; allocator requests include alignment-1. No individual block above
 * 3MiB or combined raw requests above4MiB. These are conservative format caps,
 * NOT a guarantee of available physical RAM. Context/PGA/owner are additional.
 */
#define PEAQ_PGA5S_MAGIC 0x35414750u
#define PEAQ_PGA5S_MAX_SEGMENTS 8u
#define PEAQ_PGA5S_SEGMENT_MAX (3u*1024u*1024u)
#define PEAQ_PGA5S_TOTAL_MAX (4u*1024u*1024u)
#define PEAQ_PGA5S_RUN_BYTES 384u
typedef struct {
 uint32_t magic,version,bytes,flags,segment_count,table_offset;
 uint32_t start_offset,event_offset,close_offset,poll_offset;
} PeaqPga5sDescriptor;
typedef struct { uint32_t id,bytes,alignment,flags; } PeaqPga5sSpec;
typedef struct { uint32_t id;void *data;uint32_t bytes,alignment; } PeaqPga5sMemory;
typedef struct {
 PeaqPga5sDescriptor descriptor;
 uint32_t usable_bytes,raw_bytes;
 PeaqPga5sSpec segments[PEAQ_PGA5S_MAX_SEGMENTS];
} PeaqPga5sPlan;
typedef union { uint32_t alignment;uint8_t bytes[PEAQ_PGA5S_RUN_BYTES]; } PeaqPga5sRun;
typedef int (*PeaqPga5sStart)(const PgaApi *,void *state,const PeaqPga5sMemory *,uint32_t count,uint32_t generation);
typedef void (*PeaqPga5sClose)(const PgaApi *,void *state);
typedef int (*PeaqPga5sPoll)(const PgaApi *,void *state,uint32_t milliseconds);

typedef struct {
 const uint32_t *header;uint8_t *image;void *state;PgaApi *api;
 void *context;uint32_t context_bytes,resident_base,resident_bytes;
 /* All four callbacks are trusted resident Thumb code inside resident range.
  * allocate: fallible, no assertion, UI only, NULL or fresh owned bytes.
  * deallocate: paired with allocate, synchronous, no retained references.
  * detach: clear native window state/cancel timer; a NEW list uses a different
  * Context. Reentrant CLOSE during preparation only aborts; caller must NEVER
  * free Context/image while prepare is on the stack. Once prepare returns0,
  * its segments are rolled back and Context/image ownership remains caller's.
  * release_context: after exact owner gate, ledger has freed every segment;
  * free original PGA/code/state/full Context then return into resident code.
  */
 void *(*allocate)(void *context,uint32_t raw_bytes);
 void (*deallocate)(void *context,void *allocation);
 void (*detach)(void *context,uint32_t generation);
 void (*release_context)(void *context);
} PeaqPga5sBinding;

enum { PGS_EMPTY,PGS_PREPARING,PGS_READY,PGS_RELEASING,PGS_FAILED };
enum { PGS_OK,PGS_INVALID,PGS_BUSY,PGS_OOM,PGS_ALLOCATOR,PGS_ABORTED,PGS_CLAIM };
typedef struct { uint32_t phase,error,generation,count,allocated,raw_bytes,detached; } PeaqPga5sStatus;

/* Validate without exports or allocation. 1=v2, -1=legacy ABI2/3/4, 0=invalid
 * or historical ABI5-v1. Output is changed only on complete success. */
int peaq_pga5s_validate(const uint32_t[10],const uint8_t *,PeaqPga5sPlan *);
/* Zeroed Run/API are within full Context; state matches image+BSS placement.
 * All allocations/zeroing happen in UI before claim. No exports/start/threads
 * during prepare. Any failure frees only newly allocated segments in reverse
 * order and leaves caller's Context/image untouched. Nonzero generation means
 * ownership transferred; no segment may then be freed except by resident gate.
 * A new list or a reentrant prepare cannot replace an occupied generation.
 * After failed prepare the Run is diagnostic FAILED, not silently reusable.
 */
uint32_t peaq_pga5s_prepare(PeaqPga5sRun *,PeaqResident *,const PeaqPga5sBinding *);
int peaq_pga5s_enter(PeaqPga5sRun *,PeaqResidentUse *);
int peaq_pga5s_start(PeaqPga5sRun *,const PeaqResidentUse *);
uint32_t peaq_pga5s_event(PeaqPga5sRun *,const PeaqResidentUse *,uint32_t,uint32_t);
int peaq_pga5s_leave(PeaqPga5sRun *,const PeaqResidentUse *);
/* All forced CLOSE paths MUST use this adapter, never direct owner-close.
 * Cancellation is bounded; Start/Event caller keeps lease through all old
 * Context/API accesses and releases it last. Poll via permanent owner only.
 * Start receives the actual monotonic owner generation for its native bridge.
 * Module zero-state Close/Poll must be safe even before Start. poll==1 means
 * actual native join/delete + UI file/audio/frame drain, not worker "done".
 * Doom's native binding requires peaq_doom_mocor_pump (worker_pump alone omits
 * unstarted-TCB rollback failures); Licar requires peaq_licar_io_pump.
 */
int peaq_pga5s_close(PeaqPga5sRun *);
void peaq_pga5s_status(const PeaqPga5sRun *,PeaqPga5sStatus *);

/* Explicit UI wrappers around the already-qualified original nonthrowing
 * allocator and its paired ownership-checking free. Never called in worker.
 * No allocation occurs just by linking these functions. */
void *peaq_pga5s_native_allocate(void *context,uint32_t bytes);
void peaq_pga5s_native_deallocate(void *context,void *allocation);
#endif
