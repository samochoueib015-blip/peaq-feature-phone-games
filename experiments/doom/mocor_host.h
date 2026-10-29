#ifndef PEAQ_DOOM_MOCOR_HOST_H
#define PEAQ_DOOM_MOCOR_HOST_H
#include "worker.h"
#include "../runtime/resident_tx_join.h"

#define PEAQ_DOOM_MOCOR_HOST_BYTES 1024u
#define PEAQ_DOOM_SERVICE_INTERVAL_MS 5u
typedef union { uint32_t alignment; uint8_t bytes[PEAQ_DOOM_MOCOR_HOST_BYTES]; } PeaqDoomMocorHost;
typedef struct {
 PeaqTxCreate create; PeaqTxThreadOp resume,terminate;
 PeaqTxInfo info; PeaqTxThreadOp delete_thread;
 uint32_t (*sleep)(uint32_t ticks);
 void (*sync_code)(void *,uint32_t);
} PeaqDoomMocorApi;
extern const PeaqDoomMocorApi peaq_pmp122_doom_native;
enum { PDMH_IDLE,PDMH_NO_THREAD,PDMH_RUNNING,PDMH_ROLLBACK,PDMH_QUARANTINED,PDMH_JOINED };
enum { PDMH_OK,PDMH_ARGUMENT,PDMH_CREATE,PDMH_RESUME,PDMH_IDENTITY,PDMH_ROLLBACK_ERROR,
       PDMH_PARK,PDMH_OPEN,PDMH_READ,PDMH_SEEK,PDMH_CLOSE,PDMH_SIZE,PDMH_POSITION };
typedef struct {
 uint32_t phase,error,native_status,entered,open_handles,failed_names,close_uncertain;
 uint32_t open_calls,read_calls,read_bytes,park_calls;
} PeaqDoomMocorStatus;

/* Zero storage; UI only, no native call during preparation. Caller reserves
 * this additional 1024-byte aligned4 segment before claiming resident owner.
 * All host/worker/module/heap/frame/TCB/stack allocations must be disjoint.
 * After prepare, use services in worker_prepare(generation,...,drive,"Doom",
 * iwad,episode,map). No other folder or native PC drive letter is accepted.
 * Native SFS_Open still has known OOM assertions; preallocation does not make
 * it fallible. This is an explicit experimental binding, never auto-enabled.
 */
int peaq_doom_mocor_prepare(PeaqDoomMocorHost *,const PeaqDoomMocorApi *,
 const PeaqFileApi *,PeaqDoomWorker *,const PeaqDoomMemory *,char drive);
const PeaqDoomWorkerHost *peaq_doom_mocor_services(PeaqDoomMocorHost *);
/* UI resident release gate: use THIS, not worker_pump alone. Also holds an
 * unstarted TCB whose rollback/delete failed. No callback frees any arena.
 * Worker FINISHED and zero start handle do not prove native cleanup finished.
 * Zero host state is quiescent; malformed state retains. */
int peaq_doom_mocor_pump(PeaqDoomMocorHost *,uint32_t milliseconds);
/* Optional separate resident UI service timer, independent of rendering.
 * First call is due immediately; thereafter at most one pump per 5 ms.
 * A late timer does ONE pump, never a catch-up loop. next_delay is required:
 * 1..5 schedules the next UI callback, 0 accompanies release gate ==1 only.
 * This does not install a timer or yield/run the worker inside the UI callback.
 * Its caller must provide the timer and serialize it with normal resident
 * polls/close. Actual timer resolution and SFS call latency need hardware tests.
 * Close makes the next service immediately due. Unknown join still retains. */
int peaq_doom_mocor_service(PeaqDoomMocorHost *,uint32_t milliseconds,uint32_t *next_delay);
void peaq_doom_mocor_close(PeaqDoomMocorHost *);
void peaq_doom_mocor_status(const PeaqDoomMocorHost *,PeaqDoomMocorStatus *);

/* Native task rules reuse the qualified Licar host contract: raw suspended
 * create, publish before resume, rollback ONLY never-entered suspended TCB,
 * otherwise quarantine; completed-state+identity+delete via resident_tx_join.
 * Raw worker calls only sleep, clamped1..50 native ticks (uncalibrated).
 * Every file call is synchronous UI-only, read-existing beneath D/E Games/Doom.
 * Open0 remains EIO, cached by basename to prevent a native retry in the same
 * generation; optional missing configs do not block other names. Close error
 * retains/poisons its identity forever, never retries an ambiguous handle.
 * Saves/audio remain unsupported by the worker. Physical timing is untested.
 */
#endif
