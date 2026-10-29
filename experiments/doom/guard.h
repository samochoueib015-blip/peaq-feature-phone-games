#ifndef PEAQ_DOOM_GUARD_H
#define PEAQ_DOOM_GUARD_H
#include <stdint.h>

enum { PEAQ_DOOM_FAILED=-1, PEAQ_DOOM_BAD_STATE=-2,
       PEAQ_DOOM_EXITED=0, PEAQ_DOOM_RUNNING=1 };
/* One fresh module instance per game session. Bind display and I/O first.
 * Keep argv and its strings valid for the full engine session.
 * Serialize all module exports on one worker/stack; callbacks must not reenter
 * these calls. UI input must be queued to that worker, not mutate the engine
 * concurrently with a tick.
 * They return RUNNING, EXITED (status 0), FAILED (nonzero exit), or BAD_STATE.
 * An engine exit unwinds only to the currently active call, never across
 * threads, OS callbacks, or a previously returned stack frame.
 *
 * IO_EXIT is a notification while engine code is STILL on the stack. Do not
 * free the module, heap, frames, callbacks, or handles from that callback.
 * After EXITED/FAILED returns, the host owns closing all remaining per-game
 * file handles, stopping audio and releasing its allocations. Do not call the
 * engine again; load a fresh module to start a new game session.
 * This catches exit()/I_Error/I_Quit, not CPU faults or a stuck game loop.
 */
int peaq_doom_create(int argc,char **argv);
int peaq_doom_tick(void);
int peaq_doom_quit(void);
int peaq_doom_exit_status(void); /* Read after EXITED/FAILED, before unloading. */
uint32_t peaq_doom_runtime_abi(void);

/* Internal boundary for the guarded Newlib _exit; never call from the host. */
__attribute__((noreturn)) void peaq_doom_unwind(int status);
#endif
