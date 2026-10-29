#ifndef PEAQ_LICAR_RUNTIME_H
#define PEAQ_LICAR_RUNTIME_H
#include <stdint.h>

/* Worker boundary for the existing Licar PGE1 module. No guessed phone
 * scheduler addresses are embedded here. start/join MUST be bound to a real
 * independent OS worker before this can become an installed phone package.
 * All public functions except worker_entry and the atomic stop request run on
 * one serialized UI thread. The worker may request stop after a service error.
 * The installed ABI 4 launcher releases PGA code immediately on CLOSE. It
 * cannot own this worker safely yet: first add a resident/pinned-code lifetime
 * contract that also survives forced focus/window close until join completes.
 * Merely returning from PGA_CLOSE or requesting stop is not that contract.
 */
#define PEAQ_LICAR_RUNTIME_BYTES 384u
#define PEAQ_LICAR_WORKER_STACK 32768u
typedef union { uint32_t alignment; uint8_t bytes[PEAQ_LICAR_RUNTIME_BYTES]; } PeaqLicarRuntime;
enum { PLR_EMPTY, PLR_QUEUED, PLR_LOADING, PLR_RUNNING, PLR_FINISHED, PLR_REAPED };
enum { PLR_OK, PLR_THREAD, PLR_BUSY, PLR_MODULE_OPEN, PLR_MODULE, PLR_MODULE_CLOSE,
       PLR_MEMORY, PLR_ENGINE_INIT, PLR_FRAME, PLR_AUDIO, PLR_JOIN };

typedef struct {
 void *cookie;
 /* A zero handle guarantees entry was never invoked and will never run.
  * Entry may run before start returns. A successful join means it has fully
  * returned, not merely published a completion flag. Never join by spinning.
  * join: 1 = fully joined, 0 = still running, -1 = failure (retain memory). */
 uint32_t (*start)(void *,void (*entry)(void *),void *arg,uint32_t stack_bytes);
 int (*join)(void *,uint32_t handle);
 uint32_t (*milliseconds)(void *);
 void (*sleep_ms)(void *,uint32_t);
 /* These stream methods run only on the worker. open failure must clean up
  * any partial resources; close is called exactly once after successful open.
  * read allows short reads and returns 1 on success, with a bounded count. */
 int (*module_open)(void *);
 int (*module_read)(void *,void *,uint32_t,uint32_t *);
 int (*module_close)(void *);
 /* Allocation occurs on the worker. Release occurs on the worker for a
  * rejected module, or on the UI after join; support both contexts. */
 void *(*allocate)(void *,uint32_t);
 void (*release)(void *,void *);
 void (*sync_code)(void *,void *,uint32_t);
 /* Optional per-game services, also worker-only. No open failure may be
  * interpreted as an absent save by the eventual native file adapter. */
 char (*data_next)(void *);
 int (*data_append)(void *,const char *);
 uint32_t (*music_read)(void *,uint8_t *,uint32_t);
 /* Copy, never retain, at most 800 signed PCM samples at 8 kHz. Return the
  * accepted count; backpressure is recorded instead of blocking the UI. */
 uint32_t (*audio_submit)(void *,const int16_t *,uint32_t);
 /* Mandatory: release owned file/audio services once after worker cleanup.
  * Do not release the PGE module, runtime or parent PGA in this callback. */
 void (*finish)(void *);
} PeaqLicarServices;

typedef struct { const uint16_t *pixels; uint32_t slot,sequence; } PeaqLicarFrame;
typedef struct {
 uint32_t state,error,stop_requested,paused,frames,loading_yields,steps;
 uint32_t buttons,engine_state; int32_t speed;
 uint32_t audio_samples,audio_dropped,engine_flags;
} PeaqLicarStatus;

/* Zero-initialize storage once. Reuse is allowed only after poll returns 1.
 * Services and their cookie remain valid until then. start never loads or
 * executes an engine on the UI thread. No physical devices are opened here. */
int peaq_licar_runtime_start(PeaqLicarRuntime *,const PeaqLicarServices *);
void peaq_licar_runtime_input(PeaqLicarRuntime *,uint32_t raw_message);
/* Focus loss should pause and clear input. Call resume only after the player
 * explicitly chooses to continue, not automatically on focus restoration. */
void peaq_licar_runtime_pause(PeaqLicarRuntime *,int pause);
void peaq_licar_runtime_stop(PeaqLicarRuntime *);
/* 1 = joined and all memory released, safe to unload parent PGA; 0 = busy.
 * Stop during a long engine call waits for it to return. Yield points sleep
 * on the worker; they never reenter the UI or free a live module callback. */
int peaq_licar_runtime_poll(PeaqLicarRuntime *);
void peaq_licar_runtime_status(const PeaqLicarRuntime *,PeaqLicarStatus *);
/* UI-only lease: pixels stay immutable until release, including during stop.
 * Only one lease at a time. poll cannot release memory while a lease exists. */
int peaq_licar_runtime_frame_acquire(PeaqLicarRuntime *,PeaqLicarFrame *);
int peaq_licar_runtime_frame_release(PeaqLicarRuntime *,const PeaqLicarFrame *);
#endif
