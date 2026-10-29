#ifndef PEAQ_RESIDENT_TX_JOIN_H
#define PEAQ_RESIDENT_TX_JOIN_H
#include "../licar/scheduler.h"

/* Offline-prepared raw ThreadX join adapter. No address is auto-bound and no
 * phone thread is started by this code. SCI_DeleteThread/terminate/self-exit
 * are deliberately absent. Give this only the raw TCB the owner created;
 * retain TCB, stack, name, and callback code until native delete succeeds.
 * check_resident.py also runs the real global exit hook (0x80a12275) with
 * foreign UI/timer/MIAT mutexes and a recursive worker-owned mutex. It compares
 * raw TCB pointers, never SCI pseudo-IDs; only that worker's mutex is released.
 * This bounded proof covers no waiters/inheritance, and is not a phone test.
 */
typedef struct { PeaqTxInfo info; PeaqTxThreadOp delete_thread; } PeaqResidentTxApi;
enum { PRJ_OK,PRJ_ARGUMENT,PRJ_INFO,PRJ_IDENTITY,PRJ_TERMINATED,PRJ_DELETE };
typedef struct {
 void *tcb;const char *name;PeaqResidentTxApi api;
 uint32_t joined,error,native_status,state;
} PeaqResidentTxJoin;
/* Initially zeroed state. Never reinitialize an unjoined instance. May be
 * reused after successful join, provided the old owner has no callbacks left.
 * name is the exact persistent pointer passed to raw ThreadX creation.
 */
int peaq_resident_tx_join_init(PeaqResidentTxJoin *,const PeaqResidentTxApi *,void *tcb,const char *name);
/* Compatible with host.join(cookie,handle), where handle is the owned TCB
 * pointer represented as uint32_t. Call from UI after wrapper worker_done.
 * Returns1 only after native info reports completed=1 with matching identity
 * AND raw delete returns0. Missing identity/info failure/delete failure or
 * terminated=2 returns-1 and pins everything. Other states return0. Success
 * is cached so a later poll never queries/deletes an already-deleted object.
 * This does not prove UI/audio/frame drain: resident.poll still needs those.
 */
int peaq_resident_tx_join_poll(void *cookie,uint32_t handle);
#endif
