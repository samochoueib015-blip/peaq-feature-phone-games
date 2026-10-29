#ifndef PEAQ_EXPERIMENTAL_PGA5_H
#define PEAQ_EXPERIMENTAL_PGA5_H
#include <stdint.h>
#include "../../sdk/pga.h"
#include "../runtime/resident.h"

/* EXPERIMENTAL format; the installed loader and SDK remain ABI2/3/4.
 * The 3272-byte metadata remains unchanged. ABI5 alone interprets header
 * word9 as an aligned offset into the image of this 40-byte descriptor.
 * The normal file length/hash/relocation validation MUST precede this check.
 * Descriptor fields are image-relative offsets, never relocation entries.
 * Each callback is Thumb code inside the image and outside the descriptor.
 * The old entry-offset field must equal event_offset. No game names/paths or
 * registry entries are encoded here: the selected SD folder owns its files.
 */
#define PEAQ_PGA5_MAGIC 0x35414750u
#define PEAQ_PGA5_ARENA_MAX (3u*1024u*1024u)
#define PEAQ_PGA5_RUN_BYTES 128u
typedef struct {
 uint32_t magic,version,bytes,flags,arena_bytes,arena_alignment;
 uint32_t start_offset,event_offset,close_offset,poll_offset;
} PeaqPga5Descriptor;
typedef union { uint32_t alignment;uint8_t bytes[PEAQ_PGA5_RUN_BYTES]; } PeaqPga5Run;

/* Additional arena allocated FALLIBLY by the UI before bind, aligned32 and
 * zeroed. Module state and BSS are zeroed as on legacy load. All worker TCB,
 * stack, frame, module and RPC buffers come from this declared arena; no
 * hidden allocations are permitted after ownership is transferred. Native
 * SFS/audio services stay in the UI, with their own proven cleanup gate.
 *
 * Start runs only after claim and within a lease. A return other than1 means
 * failure and requests close; it NEVER licenses immediate free. Close/Poll
 * MUST work on zeroed state before Start (forced close can arrive first).
 * Close signals cancellation, never waits/frees. Poll is bounded; exactly1
 * means true worker join/delete plus UI I/O/audio/frame drain. Neither worker
 * "done" nor returning from Close is enough. Licar delegates to io_close and
 * io_pump. This is a cooperative trusted-native contract, not a C sandbox.
 */
typedef int (*PeaqPga5Start)(const PgaApi *,void *state,void *arena,uint32_t bytes);
typedef void (*PeaqPga5Close)(const PgaApi *,void *state);
typedef int (*PeaqPga5Poll)(const PgaApi *,void *state,uint32_t milliseconds);

typedef struct {
 const uint32_t *header; /* original 10-word validated PGA header */
 uint8_t *image;void *state;PgaApi *api;void *arena;uint32_t arena_bytes;
 void *context;uint32_t context_bytes;
 /* Trusted resident launcher code range; NOT supplied by package contents. */
 uint32_t resident_base,resident_bytes;
 /* Both callbacks must be resident Thumb code within that range.
  * detach FIRST clears the native window state and cancels its timer. It may
  * create a NEW list Context, never mutate/reuse this pinned one. A detached
  * run stays reachable only from the permanent owner, not from list state.
  * release frees all arena/PGA/image/state/Context allocations exactly once;
  * it must not free the permanent owner or return into one of those blocks.
  */
 void (*detach)(void *context,uint32_t generation);
 void (*release)(void *context);
} PeaqPga5Binding;

/* 1=valid ABI5, 0=invalid, -1=legacy ABI2/3/4 (leave its old path untouched).
 * No exports are called; output is changed only on success. */
int peaq_pga5_validate(const uint32_t header[10],const uint8_t *image,PeaqPga5Descriptor *out);
/* Run and API must reside INSIDE the entire pinned Context. API.storage must
 * equal that Context; state must match the legacy image+BSS state placement.
 * Zeroed run only; no worker exists before bind. 0 leaves all ownership with
 * caller. Nonzero generation transfers the complete fixed resource group to
 * the owner. Bind never invokes any package export or native thread service.
 */
uint32_t peaq_pga5_bind(PeaqPga5Run *,PeaqResident *,const PeaqPga5Binding *);
/* The caller MUST keep this lease until every remaining launcher access to
 * the old Context/API/storage has ended. Start/event helpers do NOT release
 * it. After leave, reacquire window state; never touch the old run/cookie.
 * The token checks reject nested invocation and stale callbacks. */
int peaq_pga5_enter(PeaqPga5Run *,PeaqResidentUse *);
int peaq_pga5_start(PeaqPga5Run *,const PeaqResidentUse *);
uint32_t peaq_pga5_event(PeaqPga5Run *,const PeaqResidentUse *,uint32_t event,uint32_t value);
int peaq_pga5_leave(PeaqPga5Run *,const PeaqResidentUse *);
/* Resident forced CLOSE/Back/QUIT path: detach view before signaling stop.
 * Never legacy unload() or free. Can be reentered from a game callback.
 * Integration must use this function rather than directly closing the owner:
 * its detaching guard also protects reentrant close/poll during native view
 * cleanup when a naturally completed owner can no longer grant a game lease.
 */
int peaq_pga5_close(PeaqPga5Run *);
/* Pump only via peaq_resident_poll(permanent_owner,now). If it returns2,
 * call close (completed but still attached) then poll again. If1, reacquire
 * current view without reading the old run. Errors retain all resources.
 * Games OPEN polls the owner first; busy/error blocks a second large claim.
 */
#endif
