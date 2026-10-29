#ifndef PEAQ_RESIDENT_OWNER_H
#define PEAQ_RESIDENT_OWNER_H
#include <stdint.h>

/* One permanently resident owner for ALL large-game types. Initially zeroed;
 * never reset/reinitialize it to discard an occupied generation. These APIs
 * run only on the serialized UI task. Reentrant UI close/poll is supported;
 * concurrent calls from another task or IRQ are not. Workers use their own
 * atomic stop/channel interfaces, never this owner. No native APIs are bound.
 */
#define PEAQ_RESIDENT_BYTES 64u
typedef union { uint32_t alignment; uint8_t bytes[PEAQ_RESIDENT_BYTES]; } PeaqResident;
typedef struct {
 void *cookie;
 /* Signal cancellation only: bounded, no wait, free or native task deletion.
  * May live in the pinned allocation. Called at most once after close and
  * only outside another owner callback or UI dispatch lease. */
 void (*close)(void *);
 /* Nonblocking UI service/poll, including before the view closes. Exactly 1
  * certifies actual native join/delete + UI I/O/audio/frame-lease drain.
  * 0=busy; anything else=retain/error. A worker's finished flag alone is not
  * sufficient. May live in pinned code; must NOT free code/cookie/arenas.
  * A launch that never created a worker must still have a defined safe poll.
  * Licar's peaq_licar_io_pump is the corresponding runtime release gate. */
 int (*poll)(void *,uint32_t milliseconds);
 /* MUST be resident outside every allocation it frees. Called only after
  * close + poll==1, after poll has returned, with no UI dispatch active. Free
  * ALL owned allocations, including PGA/context/API cookie/TCB/stack. Never
  * free/reset the owner. Return before another generation can be reserved. */
 void (*release)(void *);
} PeaqResidentOps;
typedef struct { uint32_t generation,sequence; void *cookie; } PeaqResidentUse;
enum { PRG_EMPTY,PRG_ATTACHED,PRG_CLOSING,PRG_RELEASING };
enum { PRG_PENDING=0,PRG_REAPED=1,PRG_COMPLETE_ATTACHED=2,PRG_ERROR=-1,PRG_INVALID=-2 };
typedef struct {
 uint32_t generation,state,busy,ready,close_sent,lease_sequence;
 int32_t last_poll_error;
} PeaqResidentStatus;

/* Reserve ownership BEFORE starting/resuming a worker or exposing callbacks.
 * Returns a monotonically increasing nonzero generation; 0 leaves ownership
 * with caller. No wraparound/reuse after UINT32_MAX. Copy of ops is retained.
 * All resources reachable from cookie must remain pinned until release.
 * Include the old launcher Context: PgaApi.storage points inside it.
 * Prepare a safe close/poll failure path before claim, even if start fails.
 */
uint32_t peaq_resident_claim(PeaqResident *,const PeaqResidentOps *);
/* Every UI access to the pinned game (start/input/paint/frame etc.) uses one
 * lease, including any initialization after claim. At most one UI dispatch
 * may be active. End only after control returned to resident launcher code.
 * An old/duplicate lease cannot unlock a current callback or generation.
 * Losing a lease intentionally pins the generation; there is no force-free.
 */
int peaq_resident_enter(PeaqResident *,uint32_t generation,PeaqResidentUse *);
int peaq_resident_leave(PeaqResident *,const PeaqResidentUse *);
/* Caller FIRST detaches the view/window state and cancels its timer; then
 * close marks it inaccessible immediately. Never frees. A nested close while
 * dispatching defers the cancellation callback until that dispatch returns.
 * Late close events must carry the old generation, never the current one.
 */
int peaq_resident_close(PeaqResident *,uint32_t generation);
/* Call on resident Games OPEN/tick; no post-close timer is required. 1 means
 * empty/reaped; 0 pinned/busy; 2 completed but the view is still attached (it
 * must be detached/closed before release); negative retains all ownership.
 * Completion while attached blocks further game dispatch but does not free a
 * Context still referenced by the window. A second claim is always rejected
 * until release returns. Calls during a callback return pending immediately.
 */
int peaq_resident_poll(PeaqResident *,uint32_t milliseconds);
void peaq_resident_status(const PeaqResident *,PeaqResidentStatus *);

/* NARROW LOADER INTEGRATION (not installed by these sources)
 *
 * Keep ABI2/3/4 synchronous games on the existing path. Add an explicit,
 * validated large-game factory/format opt-in; a PGA claiming another ABI or
 * returning from PGA_CLOSE is NOT evidence that it safely joined a worker.
 *
 * Place one zero-initialized owner in always-resident launcher storage, never
 * in the window Context or PGA BSS. Ensure the firmware builder reserves and
 * initializes that storage. Poll on Games OPEN and before EVERY launch attempt.
 * Refuse a second game while the prior generation is attached/closing/error;
 * do not overwrite its cookie, filenames, callbacks or arenas to reuse them.
 *
 * A large factory first allocates its complete arena and a cookie with a safe
 * not-started close/poll path. claim transfers that complete ownership, enter
 * pins subsequent prepare/start/init, leave is called after control returns.
 * On preparation/start failure: detach view, close, and poll; no blind free.
 * Licar wrappers call io_close/io_pump. The latter's exact1 already includes
 * native join, UI native cleanup, pool accounting and all frame leases.
 *
 * Wrap every large-game UI access with enter/leave (start, keys, frame paint,
 * focus, volume). A lease must cover every remaining access to its old Context,
 * including the caller's post-callback work. Drop it only in resident code.
 * Return any frame lease before leave; resident drain may finish an existing
 * lease after close, but must not acquire a fresh one from a closed instance.
 *
 * On Back/PGA_QUIT/forced 0xf022/failure: cancel its timer and detach the window
 * state BEFORE close. Keep the entire old Context/PgaApi.storage and PGA image
 * alive. Returning to the list needs a fresh view Context (or separate facade
 * state), not reuse/mutation of the cookie still needed by worker I/O. A failed
 * fresh-view allocation may show the resident empty/error view; never free the
 * old run to make room. Late events carry the old generation and are rejected.
 * Do not run the legacy unload()/release(c) branch for a pinned large game.
 *
 * Release is a resident allocator callback and runs AFTER poll returns1 with
 * no outer dispatch lease. It frees the module, frames, runtime/bridge, TCB,
 * stack, pinned facade Context and any audio buffers exactly once. No callbacks
 * or native resources may retain them. After peaq_resident_poll returns1, the
 * caller must not read the old cookie; it reacquires current window state.
 *
 * No post-close timer is needed: one closed generation may stay pinned until
 * Games opens again. A future resident notification pump needs its own proven
 * cancellation/lifetime rules. The existing >=50ms PGA tick and one4096-byte
 * I/O request per tick imply at most80KiB/s (~205s for16MiB, before CPU time).
 * This gate does not solve throughput. Qualify bounded request notifications,
 * faster resident pumping or bounded batching with actual service latency
 * before claiming a playable launch time; never unbounded UI work/waiting.
 */
#endif
