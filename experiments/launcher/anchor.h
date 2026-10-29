#ifndef PEAQ_LAUNCHER_ANCHOR_H
#define PEAQ_LAUNCHER_ANCHOR_H
#include "../runtime/resident.h"

/* EXPERIMENTAL, not installed. These accessors AND their const name must live
 * permanently in the same firmware image, never in an unloadable SD module.
 * Caller: serialized normal UI task. No worker, timer, interrupt or debugger
 * calls. All entry paths must use the same accessor/name, including reopens.
 *
 * One 160-byte nonthrowing heap allocation starts with an ordinary 44-byte
 * raw ThreadX mutex. It is NEVER acquired, released or deleted: count/owner/
 * waiters remain zero. The existing kernel created-mutex list roots it until
 * reset; payload begins at offset64. The resident owner remains alive after
 * its game is reaped. There is no teardown API or mutable firmware global.
 * Reset/normal boot recreates RAM and the original mutex list; only then may
 * this allocation disappear. No hot firmware replacement is supported.
 *
 * An IRQ-masked bounded list scan validates all links before returning any
 * existing instance. Allocations/frees occur after restoring IRQ state. A
 * second scan before publication handles allocation-time reentrant OPEN.
 * Native create is allocation-free. An unexpected list, hook, owner, name-
 * identity collision, task context or allocation failure fails closed.
 * Native IRQ control saves/restores both I/F control bits, but its masking
 * call clears F temporarily: calls with F already masked are rejected before
 * changing it (see anchor_sync.S). This is NOT an SMP/FIQ registry design.
 */
#define PEAQ_LAUNCHER_ANCHOR_BYTES 160u
#define PEAQ_LAUNCHER_ANCHOR_MAX_MUTEXES 256u
enum {
 PLA_OK,PLA_CONTEXT,PLA_REGISTRY,PLA_LIMIT,PLA_OOM,PLA_NATIVE_CREATE
};
PeaqResident *peaq_launcher_anchor_owner(uint32_t *error);

/* Exposed only for review/test of the persistent identity. Not an API to
 * create another anchor. The returned pointer is inside resident .rodata. */
const char *peaq_launcher_anchor_name(void);
#endif
