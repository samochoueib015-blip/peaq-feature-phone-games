#ifndef PEAQ_LICAR_IO_CHANNEL_H
#define PEAQ_LICAR_IO_CHANNEL_H
#include <stdint.h>

/* One worker, one serialized UI owner, at most one in-flight request. Every
 * byte is copied through fixed storage; no asynchronous native operation may
 * retain a worker-stack pointer. The channel and ALL callback code must remain
 * resident/pinned through native join and UI drain, including forced close.
 */
#define PEAQ_LICAR_IO_PAYLOAD 4096u
#define PEAQ_LICAR_CHANNEL_BYTES 4224u
typedef union { uint32_t alignment; uint8_t bytes[PEAQ_LICAR_CHANNEL_BYTES]; } PeaqLicarChannel;
typedef struct { uint32_t generation,sequence; } PeaqLicarTicket;
enum { PLIC_MODULE_OPEN=1,PLIC_MODULE_READ,PLIC_MODULE_CLOSE,PLIC_DATA_READ,
       PLIC_DATA_APPEND,PLIC_MUSIC_READ,PLIC_AUDIO_WRITE,PLIC_SYNC_CODE };
enum { PLIC_PENDING=0,PLIC_DONE=1,PLIC_CANCELED=-1,PLIC_STALE=-2,
       PLIC_INVALID=-3,PLIC_BUSY=-4,PLIC_EXHAUSTED=-5 };
enum { PLIC_IDLE,PLIC_BUILDING,PLIC_REQUEST,PLIC_UI_OWNED,
       PLIC_RESPONDING,PLIC_RESPONSE,PLIC_READING };
typedef struct {
 PeaqLicarTicket ticket;
 uint32_t operation,arg0,arg1,bytes,output_limit;
 uint8_t data[PEAQ_LICAR_IO_PAYLOAD];
} PeaqLicarRequest;
typedef struct { int32_t value; uint32_t bytes; uint8_t data[PEAQ_LICAR_IO_PAYLOAD]; } PeaqLicarResponse;
typedef struct {
 uint32_t generation,sequence,state,closed,worker_done,joined,drained,requests,responses;
} PeaqLicarChannelStatus;

/* UI: initially zeroed storage; generation nonzero and strictly increasing.
 * Reuse additionally requires can_release from the prior generation. */
int peaq_licar_channel_init(PeaqLicarChannel *,uint32_t generation);
/* Worker: submission copies input and returns a non-reusable ticket. Sequence
 * exhaustion closes the generation rather than allowing an ABA/wraparound. */
int peaq_licar_channel_submit(PeaqLicarChannel *,uint32_t operation,uint32_t arg0,
 uint32_t arg1,const void *input,uint32_t bytes,uint32_t output_limit,PeaqLicarTicket *);
/* Worker: DONE consumes the response. Stale/invalid replies do not touch out.
 * Cancellation may finish the worker's wait before the UI operation finishes;
 * that never releases storage still owned by the UI. No callback is invoked. */
int peaq_licar_channel_result(PeaqLicarChannel *,PeaqLicarTicket,
 int32_t *value,void *output,uint32_t capacity,uint32_t *bytes);
/* UI: take copies into UI-owned storage, which remains valid for the native
 * operation. Respond only after it no longer retains request/response buffers.
 * Stale, duplicate or oversized responses are rejected without altering state.
 * A matching response after cancellation drains ownership, discards its data,
 * and returns CANCELED. abandon is for a native operation already quiescent. */
int peaq_licar_channel_take(PeaqLicarChannel *,PeaqLicarRequest *);
int peaq_licar_channel_respond(PeaqLicarChannel *,PeaqLicarTicket,int32_t value,
 const void *data,uint32_t bytes);
int peaq_licar_channel_abandon(PeaqLicarChannel *,PeaqLicarTicket);
/* UI close/stop never frees storage and never waits. */
void peaq_licar_channel_close(PeaqLicarChannel *);
int peaq_licar_channel_closed(const PeaqLicarChannel *);
/* Worker wrapper: call after the engine entry returns. Still NOT native join. */
void peaq_licar_channel_worker_done(PeaqLicarChannel *);
/* UI: 1 only when native join confirms actual thread completion, 0 busy,
 * -1 error. No callback runs until worker_done. A failed join retains state. */
int peaq_licar_channel_join(PeaqLicarChannel *,int (*poll_join)(void *,uint32_t),void *,uint32_t handle);
/* UI: ONLY after a failed native start that guarantees entry never ran and
 * cannot later run. This is not a way to cancel an existing thread. */
void peaq_licar_channel_no_worker(PeaqLicarChannel *);
/* UI: discard unclaimed requests/replies after close, never UI-owned accesses.
 * Mark drained only once native file/audio cleanup has actually completed. */
int peaq_licar_channel_idle(PeaqLicarChannel *);
int peaq_licar_channel_drained(PeaqLicarChannel *);
int peaq_licar_channel_can_release(PeaqLicarChannel *);
void peaq_licar_channel_status(const PeaqLicarChannel *,PeaqLicarChannelStatus *);
#endif
