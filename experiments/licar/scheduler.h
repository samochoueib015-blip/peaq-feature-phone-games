#ifndef PEAQ_LICAR_SCHEDULER_RESEARCH_H
#define PEAQ_LICAR_SCHEDULER_RESEARCH_H
#include <stdint.h>

/* ORIGINAL-FIRMWARE RESEARCH CONTRACT, NOT AN ENABLED PHONE BINDING.
 * Qualified by tools/check_licar_scheduler.py against kernel SHA-256:
 * 071044abfad362aea58eba0f01832d98eba6e10899cc0cd0bf9a9379946e9c79
 * Original ARM executes under bounded backend stubs with write guards enabled.
 * No real worker or physical SD/audio service has been exercised from it.
 * These constants must never silently enable a launcher feature.
 */
#define PEAQ_SCHEDULER_PHONE_QUALIFIED 0
#define PEAQ_SCI_CREATE_THREAD_THUMB 0x810abe57u
#define PEAQ_SCI_CREATE_APP_THREAD_THUMB 0x810ac2c9u
#define PEAQ_SCI_RESUME_THREAD_THUMB 0x810ac7c9u
#define PEAQ_SCI_GET_THREAD_INFO_THUMB 0x810aca97u
#define PEAQ_SCI_DELETE_THREAD_THUMB 0x810aae3bu
#define PEAQ_SCI_SLEEP_THUMB 0x810acc7du
#define PEAQ_SCI_TICKS_THUMB 0x810ae38bu
#define PEAQ_SCI_SET_THREAD_EVENT_THUMB 0x810accbbu
#define PEAQ_SCI_GET_THREAD_EVENT_THUMB 0x810accffu
#define PEAQ_SCI_IDENTIFY_THREAD_THUMB 0x810ac67du
#define PEAQ_SCI_SEND_SIGNAL_THUMB 0x810abd01u
#define PEAQ_SCI_SELF_EXIT_THUMB 0x810ac87fu
#define PEAQ_SCI_SUSPEND_THREAD_THUMB 0x810ac725u
#define PEAQ_TX_CREATE_THUMB 0x80d7d631u
#define PEAQ_TX_RESUME_THUMB 0x80a07f4du
#define PEAQ_TX_INFO_THUMB 0x80a07ef5u
#define PEAQ_TX_DELETE_THUMB 0x80a07ec5u
#define PEAQ_TX_TCB_BYTES 188u
#define PEAQ_TX_MIN_STACK_BYTES 200u

enum PeaqNativeThreadState {
 PEAQ_THREAD_READY=0, PEAQ_THREAD_COMPLETED=1, PEAQ_THREAD_TERMINATED=2,
 PEAQ_THREAD_SUSPENDED=3, PEAQ_THREAD_SLEEPING=4, PEAQ_THREAD_EVENT_WAIT=7
};
enum PeaqNativeEventOption {
 PEAQ_EVENT_OR=0, PEAQ_EVENT_OR_CLEAR=1,
 PEAQ_EVENT_AND=2, PEAQ_EVENT_AND_CLEAR=3
};

/* SCI owns a 300-byte table record: ThreadX TCB at +8, queue +0xc4,
 * event group +0xfc, pending signals +0x11c, stack allocation +0x124,
 * name/stack-size metadata allocation +0x128. The creator reserves a dynamic
 * ID >=111 and makes three heap allocations: stack + 4*queue_entries, 12-byte
 * entry bootstrap, 12-byte metadata. No private record may be fabricated to
 * bypass the reservation/accounting functions.
 *
 * Both SCI creators can ASSERT fatally on invalid parameters, no free slots,
 * or allocation failure. The second allocation is even dereferenced without
 * a null check. DO NOT bind either one to UI start as a fallible constructor.
 * A returned dynamic ID is not a pointer. 0xffffffff is invalid. Names and
 * argv must outlive the worker. queue_entries must be nonzero. Creation with
 * auto_start=0 leaves state=3; publish ownership before calling resume.
 * preempt=1 passes priority as the ThreadX preemption threshold; 0 passes 0.
 */
typedef void (*PeaqSciEntry)(uint32_t argc,void *argv);
typedef uint32_t (*PeaqSciCreate)(const char *name,const char *queue_name,
 PeaqSciEntry entry,uint32_t argc,void *argv,uint32_t stack_bytes,
 uint32_t queue_entries,uint32_t priority,uint32_t preempt,uint32_t auto_start);
typedef uint32_t (*PeaqSciThreadOp)(uint32_t id);
/* name/queue_name may be NULL. Non-NULL strings are copied WITHOUT bounds by
 * the original wrapper, so the join path must use NULL for both. state,
 * priority and preempt must all point to writable uint32_t values.
 * 0=success; missing ID=0xff with outputs untouched. Other native failures
 * can assert. Resume returns 0x12 when the task is not suspended.
 */
typedef uint32_t (*PeaqSciInfo)(uint32_t id,char *name,char *queue_name,
 uint32_t *state,uint32_t *priority,uint32_t *preempt);
typedef void (*PeaqSciSleep)(uint32_t duration);
typedef uint32_t (*PeaqSciTicks)(void);
typedef uint32_t (*PeaqSciEventSet)(uint32_t id,uint32_t bits,uint32_t option);
typedef uint32_t (*PeaqSciEventGet)(uint32_t id,uint32_t bits,uint32_t option,
 uint32_t *actual,uint32_t wait);
typedef uint32_t (*PeaqSciSendSignal)(void *signal,uint32_t target_id);
typedef void (*PeaqSciSelfExit)(void);
/* Set allows OR/AND only. Missing-ID set misleadingly returns 0; never use
 * that success as evidence of a live task. Get accepts all four options;
 * actual receives the full flag word, not merely requested bits. wait=0 polls,
 * 0xffffffff waits indefinitely, otherwise the duration is passed unchanged.
 * Empty/timeout gives SCI status 7 without populating actual. Event wait must
 * never be used on the UI thread. Sleep outside normal task context busy-waits.
 * The native counter reads 0x4023000c; physical tick calibration remains open.
 * Signal send enqueues the POINTER with wait=0, without copying payload bytes.
 * Missing target returns 0xff; NULL message or full queue causes an assertion.
 * Successful enqueue therefore requires transferred/stable payload ownership.
 * Self-exit allocates a 20-byte message (ID 0xfff1, caller ID at +12), sends it
 * to task 110, then suspends the caller. It is NOT a synchronous join/delete.
 */

/* Direct ThreadX creation is a fallible alternative. The caller owns both
 * buffers; creation initializes exactly 188 TCB bytes and does not allocate
 * SCI queue/event/metadata. All entries take one uint32_t input. Use suspended
 * start=0. The real shell calls entry(input), then sets state=1, then yields
 * permanently to the scheduler. The caller must preserve the name as well.
 *
 * Return codes: invalid TCB=0x0e; null entry/stack=3; stack below 200 bytes=5;
 * priority>255=0x0f; threshold>priority=0x18; start>1=0x10; bad context=0x13.
 * No error above invokes SCI_Assert. Host allocation must still be nonthrowing.
 * A raw TCB has NO usable SCI dynamic ID. Crucially, SCI_IdentifyThread aliases
 * such a thread to 0x4154494d (MIAT/timer pseudo-ID), even with readable prefix
 * bytes before the TCB. Therefore SCI file/audio compatibility is NOT proven;
 * do not attach existing SCI ID-based queue/event services to a raw worker.
 */
typedef void (*PeaqTxEntry)(uint32_t input);
typedef uint32_t (*PeaqTxCreate)(void *tcb,const char *name,PeaqTxEntry entry,
 uint32_t input,void *stack,uint32_t stack_bytes,uint32_t priority,
 uint32_t preemption_threshold,uint32_t time_slice,uint32_t auto_start);
typedef uint32_t (*PeaqTxThreadOp)(void *tcb);
typedef uint32_t (*PeaqTxInfo)(void *tcb,char **name,uint32_t *state,
 uint32_t *run_count,uint32_t *priority,uint32_t *preemption_threshold,
 uint32_t *time_slice,void **next_created,void **first_suspended);
/* Raw info requires ALL eight output pointers. Raw delete accepts states 1/2
 * only, returns 0x11 for live/suspended/sleeping/waiting states, and unlinks the
 * object without freeing caller storage. Our host must only accept state=1,
 * because terminated=2 does not prove that game callbacks returned normally.
 */

/* REQUIRED RESIDENT/PINNED LIFETIME PLAN
 *
 * 1. A resident launcher manager owns one launch generation, the runtime,
 *    engine module, all service callbacks and their cookies, worker name,
 *    frame buffers, files and audio drain state. No worker callback may point
 *    into a disposable PGA/window allocation. Reserve all ownership before
 *    resume. The installed immediate-PGA-free-on-CLOSE ABI4 is insufficient.
 * 2. The UI facade reads frame leases and posts input/stop. Closing or forced
 *    focus loss detaches the view and requests stop; it never kills the task,
 *    frees live memory, invokes engine exports or waits synchronously.
 * 3. The worker completes its current engine call, closes/drains services,
 *    sets its own finished flag and RETURNS. It must not call SCI_ExitThread
 *    (which posts an asynchronous deletion signal then suspends itself).
 * 4. Resident code polls OWNED identity only. If info fails, ID disappears,
 *    state=2, or identity is uncertain, retain allocations and report error.
 *    Finished flag + successful native info with state=1 proves return through
 *    the native shell. Then native delete must succeed. No missing-ID shortcut.
 * 5. Drain UI frame leases and audio ownership, then release module/runtime/
 *    callback code. Raw ThreadX additionally requires freeing caller TCB/stack;
 *    SCI delete frees its own three allocations. Its termination hook may
 *    notify twice (entry return and later delete), so it is NOT our finish API.
 *
 * A first bounded implementation needs no new timer callback after UI close:
 * keep at most ONE closed generation pinned, and poll/reap when Games next
 * opens. Refuse a second launch while it is still completing. That temporarily
 * retains memory but survives a forced window close without dangling code.
 * An independent resident timer can improve timely collection only after its
 * own cancellation/lifetime ABI is qualified. Original callbacks cannot be
 * freed merely because the MMI window or the worker's finished flag is gone.
 */
#endif
