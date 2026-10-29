#ifndef PEAQ_LICAR_IO_BRIDGE_H
#define PEAQ_LICAR_IO_BRIDGE_H
#include "io_channel.h"
#include "runtime.h"

#define PEAQ_LICAR_IO_BRIDGE_BYTES 18432u
#define PEAQ_LICAR_IO_MODULE_BYTES (1024u*1024u+31u)
#define PEAQ_LICAR_IO_FRAME_BYTES (4u*240u*320u*2u+31u)
#define PEAQ_LICAR_IO_TCB_BYTES (8u+188u)
typedef union { uint32_t alignment;uint8_t bytes[PEAQ_LICAR_IO_BRIDGE_BYTES]; } PeaqLicarIoBridge;
typedef struct {
 void *module;uint32_t module_bytes;
 void *frames;uint32_t frame_bytes;
 void *tcb;uint32_t tcb_bytes;
 void *stack;uint32_t stack_bytes;
} PeaqLicarIoMemory;
typedef struct {
 void *cookie;
 /* UI only. These buffers were already allocated by the UI. start must use
  * a fallible raw-ThreadX constructor, not assert-prone SCI_CreateThread.
  * No handle on failure guarantees entry never ran and can never run.
  * join must test actual OS completed state, delete the owned TCB and return
  * 1; 0=busy, -1=retain/retry. It does not free caller-owned TCB/stack memory. */
 uint32_t (*start)(void *,void (*entry)(void *),void *arg,void *tcb,
                  uint32_t tcb_bytes,void *stack,uint32_t stack_bytes);
 int (*join)(void *,uint32_t handle);
 /* Worker only: bounded raw scheduler wait, not SCI_*, SFS, allocation or UI.
  * It must not call back into the game. No busy-spin while the UI is absent. */
 void (*park)(void *,uint32_t duration);
 /* UI only. Exactly one bounded request per pump, at most 4096 payload bytes.
  * Returns 1 when handled; native success/failure goes in response.value.
  * All buffers are copied/pinned; perform must finish using them before it
  * returns. It must not wait for a new worker request or reenter the game.
  * MODULE/DATA/MUSIC paths are fixed by this host, never supplied by worker.
  * Empty DATA_APPEND means rewind (including the bridge prefetch cache).
  * OPEN/CLOSE/APPEND/SYNC use value=1 on success, no output bytes. READ uses
  * value=1 plus bytes, including successful EOF=0 bytes. AUDIO uses accepted
  * sample count <= arg0, no bytes, and returns 0 immediately on backpressure.
  * SYNC uses arg0 offset within memory.module and arg1 length, never a pointer.
  * The byte/call budget does NOT guarantee physical SFS operation latency. */
 int (*perform)(void *,const PeaqLicarRequest *,PeaqLicarResponse *);
 /* UI only, after engine entry has returned and no UI request is in flight.
  * Close every owned file and drain/discard native queued audio. 1=fully
  * quiescent, 0=poll again, -1=retain/error. Must be idempotent across polls.
  * Do not free the arena or any code here; pump returning 1 is the release gate. */
 int (*cleanup)(void *);
} PeaqLicarIoHost;
typedef struct {
 PeaqLicarChannelStatus channel;
 uint32_t error,finish_seen,cleanup_attempts,pool_claims,started;
} PeaqLicarIoStatus;
enum { PLIO_OK,PLIO_REQUEST,PLIO_BACKEND,PLIO_RESPONSE,PLIO_SYNC,PLIO_POOL,PLIO_CLEANUP };

/* UI preparation only. Caller has allocated bridge/runtime/module/frame/TCB/
 * stack memory already, with no overlap. Names and callback code live in a
 * resident manager. No native API, thread or file is invoked by prepare.
 * Use services as the io argument of existing peaq_licar_runtime_start().
 * This adapter's finish callback requests deferred UI cleanup; its release
 * callbacks only relinquish pool claims. runtime_poll()==1 alone therefore
 * NEVER authorizes freeing an arena. Only io_pump()==1 is the complete gate. */
int peaq_licar_io_prepare(PeaqLicarIoBridge *,uint32_t generation,PeaqLicarRuntime *,
 const PeaqLicarIoHost *,const PeaqLicarIoMemory *);
const PeaqLicarServices *peaq_licar_io_services(PeaqLicarIoBridge *);
/* Serialized UI only. Updates the worker's cached clock, services at most one
 * request, polls completion/cleanup. 1 means all six arenas/code may be
 * released: native join + UI drain + runtime frame-lease cleanup are complete.
 * It never waits for the worker. Keep manager/code pinned when returning 0.
 * After closing, this can be called when Games next opens; no new launch is
 * allowed while a previous generation remains pinned. */
int peaq_licar_io_pump(PeaqLicarIoBridge *,uint32_t milliseconds);
/* UI stop/close or worker abort after a failed raw wait. Only atomic
 * cancellation, no native calls/free; never releases an owned UI request. */
void peaq_licar_io_close(PeaqLicarIoBridge *);
void peaq_licar_io_status(const PeaqLicarIoBridge *,PeaqLicarIoStatus *);
#endif
