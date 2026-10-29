#ifndef PEAQ_DOOM_WORKER_H
#define PEAQ_DOOM_WORKER_H
#include "fileio.h"
#include "../licar/io_channel.h"

/* Guarded rev2, 2 MiB zone within a 2.5 MiB Newlib heap. All these independent
 * allocations are made fallibly by the UI BEFORE start. No native allocation,
 * filesystem, audio or SCI service is allowed in the raw worker. */
#define PEAQ_DOOM_WORKER_BYTES 24576u
#define PEAQ_DOOM_MODULE_BYTES (627468u+31u)
#define PEAQ_DOOM_HEAP_BYTES (2560u*1024u+7u)
#define PEAQ_DOOM_FRAME_BYTES (3u*240u*320u*2u+31u)
#define PEAQ_DOOM_TCB_BYTES (8u+188u)
#define PEAQ_DOOM_STACK_BYTES 32768u
typedef union { uint32_t alignment; uint8_t bytes[PEAQ_DOOM_WORKER_BYTES]; } PeaqDoomWorker;
typedef struct {
 void *module; uint32_t module_bytes;
 void *heap; uint32_t heap_bytes;
 void *frames; uint32_t frame_bytes;
 void *tcb; uint32_t tcb_bytes;
 void *stack; uint32_t stack_bytes;
} PeaqDoomMemory;
typedef struct {
 void *cookie;
 /* Serialized UI only. Zero guarantees entry never ran and can never run.
  * Nonzero may be returned after entry already ran. join returns exactly 1
  * only after real completed-state verification and deletion of this TCB;
  * 0 busy, -1 error/retain. Neither callback frees caller-owned allocations. */
 uint32_t (*start)(void *,void (*)(void *),void *,void *,uint32_t,void *,uint32_t);
 int (*join)(void *,uint32_t);
 /* Raw worker only. Bounded scheduler wait, no reentrant UI/engine callback.
  * Requested delays are 1..50; the native tick duration needs calibration.
  * On a wait error the host may call worker_close(), then must still return. */
 void (*park)(void *,uint32_t);
 /* UI only. Complete instruction/data-cache synchronization before return.
  * 1 success, other value aborts loading before any engine entry executes. */
 int (*sync_code)(void *,void *,uint32_t);
 const PeaqFileApi *files;
} PeaqDoomWorkerHost;

enum { PDW_EMPTY,PDW_PREPARED,PDW_QUEUED,PDW_LOADING,PDW_RUNNING,PDW_FINISHED,PDW_REAPED };
enum { PDW_OK,PDW_THREAD,PDW_MODULE,PDW_SYNC,PDW_REQUEST,PDW_RESPONSE,
       PDW_ENGINE,PDW_FRAME,PDW_JOIN,PDW_CLOSE_UNCERTAIN };
typedef struct { const uint16_t *pixels; uint32_t generation,slot,sequence; } PeaqDoomFrame;
typedef struct {
 uint32_t state,error,stop,paused,frames,steps,buttons,heap_used;
 uint32_t exit_seen; int32_t exit_status;
 uint32_t close_uncertain,cleanup_calls;
 PeaqLicarChannelStatus channel;
} PeaqDoomWorkerStatus;

/* Serialized UI. Initially zero storage; reuse requires previous pump()==1
 * and a strictly greater nonzero generation. Copies host/memory/API/filename.
 * Files live in exactly drive:\Games\folder; module name is engine.pge.
 * iwad must be one valid basename. Only trusted qualified PGE code is used;
 * this is a lifetime boundary, not a sandbox for untrusted ARM instructions. */
int peaq_doom_worker_prepare(PeaqDoomWorker *,uint32_t generation,
 const PeaqDoomWorkerHost *,const PeaqDoomMemory *,char drive,const char *folder,
 const char *iwad,uint32_t episode,uint32_t map);
/* 1 worker started; 0 invalid/busy, no worker; -1 native start failed and the
 * caller must still pump to release the claimed generation. One global owner
 * remains reserved until join+UI drain+frame release, not merely worker exit. */
int peaq_doom_worker_start(PeaqDoomWorker *);
/* Serialized UI: at most ONE copied request (<=4 KiB), or ONE cleanup close.
 * Never waits for the worker. Exactly 1 is the only allocation/code release
 * gate: real native join, no in-flight I/O, all file handles closed, no lease.
 * Initially zero storage is quiescent; prepare+close before start also drains
 * without launching a worker. Reentrant pumps return 0. Poll again on Games
 * reopening after forced close.
 * At one pump/50 ms, raw read throughput is capped at ~80 KiB/s; this is NOT
 * a qualified gameplay cadence. SFS call latency itself is not bounded here. */
int peaq_doom_worker_pump(PeaqDoomWorker *,uint32_t now_ms);
/* Atomic close/abort, usable from UI or worker error path. No free/OS calls. */
void peaq_doom_worker_close(PeaqDoomWorker *);
/* UI supplies CURRENT logical bits from platform.h (forward/back/strafe/
 * fire/turn/use/menu/accept). Quick press edges survive until one worker tick.
 * Caller maps rotated handset keys; bit4 (jump) has no vanilla Doom action. */
void peaq_doom_worker_buttons(PeaqDoomWorker *,uint32_t bits);
void peaq_doom_worker_pause(PeaqDoomWorker *,int pause);
void peaq_doom_worker_status(const PeaqDoomWorker *,PeaqDoomWorkerStatus *);
int peaq_doom_worker_frame_acquire(PeaqDoomWorker *,PeaqDoomFrame *);
int peaq_doom_worker_frame_release(PeaqDoomWorker *,const PeaqDoomFrame *);

/* Reuses the proven Licar transport, WITHOUT Licar's operation semantics:
 * 1/2/3 = module open/read/close; 4 = Doom read-only syscall; 8 = code sync.
 * For syscall: arg0=DOOM_IO_*, arg1=fd or OPEN flags. OPEN/STAT input is a
 * copied NUL-terminated basename (<=32 B); SEEK input is offset+origin (8 B);
 * READ has output_limit<=4096. No engine pointer crosses this boundary.
 * UI calls the existing DoomFiles adapter. Native close errors quarantine the
 * instance permanently (no retry on an ambiguously closed/reused handle).
 * Audio is explicitly disabled; saves/writes return EROFS. Not phone-tested. */
#endif
